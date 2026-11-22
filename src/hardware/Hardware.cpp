#include "Hardware.h"
#include <SimpleFOC.h>
#include <SCServo.h>
#include <MPU6050_tockn.h>
#include <esp_adc_cal.h>
#include <driver/adc.h>
namespace hardware {
// 构造依赖同一翻译单元，原硬件引脚与对象顺序保持。
BLDCMotor m0(7), m1(7);
BLDCDriver3PWM d0(32, 33, 25, 22), d1(26, 27, 14, 12);
TwoWire bus0(0), bus1(1);
MagneticSensorI2C s0(AS5600_I2C), s1(AS5600_I2C);
SMS_STS servo;
MPU6050 imu(bus0);
esp_adc_cal_characteristics_t adc;
float battery = 0, percent = 0;
bool low = true, valid = false;
uint32_t batteryMs = 0;
// 只读电池有效性、低电锁与2秒新鲜度；不在保护路径等待ADC。
bool batteryOk() {
    return valid && !low && uint32_t(millis() - batteryMs) <= 2000;
}
float batteryPercent() {
    return percent;
}
// 分摊8次ADC采样；5ms间隔，500ms批次间隔，20%/25%回差。
void maintainBattery() {
    static uint32_t last = 0, point = 0, sum = 0;
    static unsigned count = 0;
    uint32_t now = millis();
    if (!count && valid && uint32_t(now - last) < 500)
        return;
    if (count && uint32_t(now - point) < 5)
        return;
    sum += adc1_get_raw(ADC1_CHANNEL_7);
    point = now;
    if (++count < 8)
        return;
    battery = esp_adc_cal_raw_to_voltage(sum / 8, &adc) * 3.075f / 1000.f;
    sum = count = 0;
    last = batteryMs = now;
    valid = true;
    percent = constrain((battery - 6.6f) / 1.5f * 100, 0.f, 100.f);
    if (percent < 20)
        low = true;
    else if (percent > 25)
        low = false;
}
// 唯一硬件初始化所有者；保留对象依赖顺序与轮FOC对齐流程。
bool begin() {
    Serial.println("[BOOT] servo off");
    Serial2.begin(1000000);
    servo.pSerial = &Serial2;
    servo.EnableTorque(1, 0);
    servo.EnableTorque(2, 0);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_12);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 0, &adc);
    Serial.println("[BOOT] i2c begin");
    bus0.begin(19, 18, 400000);
    bus1.begin(23, 5, 400000);
    Serial.println("[BOOT] encoder0 begin");
    s0.init(&bus0);
    Serial.println("[BOOT] encoder1 begin");
    s1.init(&bus1);
    Serial.println("[BOOT] imu begin");
    imu.begin();
    Serial.println("[BOOT] imu calibrate");
    imu.calcGyroOffsets(false);
    Serial.println("[BOOT] imu ready; drivers begin");
    m0.linkSensor(&s0);
    m1.linkSensor(&s1);
    d0.voltage_power_supply = d1.voltage_power_supply = 8;
    d0.init();
    d1.init();
    m0.linkDriver(&d0);
    m1.linkDriver(&d1);
    for (BLDCMotor *m : {&m0, &m1}) {
        m->PID_velocity.P = .05;
        m->PID_velocity.I = 1;
        m->PID_velocity.D = 0;
        m->voltage_sensor_align = 6;
        m->torque_controller = TorqueControlType::voltage;
        m->controller = MotionControlType::torque;
        m->init();
    }
    // 对齐属于启动初始化，完成立即失能；不启动平衡或AI。
    Serial.println("[BOOT] foc0 begin");
    bool ok0 = m0.initFOC();
    m0.disable();
    Serial.printf("[BOOT] foc0 ok=%d direction=%d zero=%.6f\n", ok0, int(m0.sensor_direction),
                  m0.zero_electric_angle);
    Serial.println("[BOOT] foc1 begin");
    bool ok1 = m1.initFOC();
    m1.disable();
    Serial.printf("[BOOT] foc1 ok=%d direction=%d zero=%.6f; wheels disabled\n", ok1,
                  int(m1.sensor_direction), m1.zero_electric_angle);
    return ok0 && ok1;
}
// 读取原始IMU角度/角速度（度、度每秒），轮角/轮速为rad、rad/s。
control::Observation sample(bool ai) {
    imu.update();
    if (ai) {
        s0.update();
        m0.shaft_angle = m0.shaftAngle();
        m0.shaft_velocity = m0.shaftVelocity();
        s1.update();
        m1.shaft_angle = m1.shaftAngle();
        m1.shaft_velocity = m1.shaftVelocity();
    }
    control::Observation o;
    o.nowUs = micros();
    o.pitch = imu.getAngleY();
    o.roll = imu.getAngleX();
    o.gyroX = imu.getGyroX();
    o.gyroY = imu.getGyroY();
    o.gyroZ = imu.getGyroZ();
    o.accZ = imu.getAccZ();
    o.battery = battery;
    o.wheelAngle[0] = m0.shaft_angle;
    o.wheelAngle[1] = m1.shaft_angle;
    o.wheelVelocity[0] = m0.shaft_velocity;
    o.wheelVelocity[1] = m1.shaft_velocity;
    return o;
}
// 唯一执行出口；输入保护后目标，回写实际提交舵机计数和轮目标；不等待网络。
void apply(control::Targets &t, bool ai, bool active, float *focQ) {
    static bool torque = false, enabled = false;
    static int last[2] = {-1, -1};
    if (active != enabled) {
        if (active) {
            m0.voltage.q = m1.voltage.q = 0;
            m0.voltage.d = m1.voltage.d = 0;
            m0.target = m1.target = 0;
            m0.enable();
            m1.enable();
        } else {
            m0.voltage.q = m1.voltage.q = 0;
            m0.disable();
            m1.disable();
        }
        enabled = active;
    }
    if (t.torque != torque) {
        servo.EnableTorque(1, t.torque);
        servo.EnableTorque(2, t.torque);
        torque = t.torque;
        last[0] = last[1] = -1;
    }
    if (torque && (abs(int(t.leg[0]) - last[0]) >= 3 || abs(int(t.leg[1]) - last[1]) >= 3)) {
        uint8_t ids[2] = {1, 2}, acc[2] = {t.acceleration, t.acceleration};
        int16_t pos[2] = {int16_t(t.leg[0]), int16_t(t.leg[1])};
        uint16_t speeds[2] = {t.speed, t.speed};
        servo.SyncWritePosEx(ids, 2, pos, speeds, acc);
        last[0] = pos[0];
        last[1] = pos[1];
    }
    if (torque) {
        t.leg[0] = last[0];
        t.leg[1] = last[1];
    }
    m0.target = active ? t.wheel[0] : 0;
    m1.target = active ? t.wheel[1] : 0;
    if (ai && active) {
        m0.voltage.q = m0.target;
        m1.voltage.q = m1.target;
        m0.voltage.d = m1.voltage.d = 0;
    }
    focQ[0] = m0.voltage.q;
    focQ[1] = m1.voltage.q;
    m0.loopFOC();
    m1.loopFOC();
    if (!ai) {
        m0.move();
        m1.move();
    }
    t.wheel[0] = m0.target;
    t.wheel[1] = m1.target;
}
} // namespace hardware
