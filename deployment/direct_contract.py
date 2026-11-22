"""源码构建合同验证；不代替冻结训练包的独立来源审计。"""

import hashlib, json

FORMAT = "direct-wheel-voltage-v1"
REPAIR_CONFIG = {
    "seed": 1982305524,
    "angle_noise_rad": 0.02,
    "failure_rate": 10.290214713064932,
    "training_approved": True,
    "clean_removed": ["teacher", "episode_roll_bias", "leg_execution_height_error"],
    "world_count": 64,
    "horizon": 256,
    "optimization_epochs": 5,
    "minibatch_size": 2048,
    "gamma": 0.99,
    "gae_lambda": 0.95,
    "learning_rate": 0.0003,
    "entropy_coefficient": 0.005,
    "value_coefficient": 0.5,
    "clip": 0.2,
    "action_delay_ms": [7, 7],
    "initialization": "inherit current scratch model/Adam; cold clean env, no teacher or episode sensor/execution offsets",
}
EXPECTED = {
    "version": "direct-wheel-voltage-v1",
    "action_order": [
        "left_leg_position",
        "right_leg_position",
        "left_wheel_voltage",
        "right_wheel_voltage",
    ],
    "action_dim": 4,
    "leg_target": "clip(command_height + .12*a, -.5,-.12) rad, not measured qpos",
    "wheel_target": "reserved17/18 always zero",
    "wheel_inner": "U_model=7.4*clip(a,-1,1) V; hold until accepted action; no wheel LPF/P",
    "control_dt_s": 0.01,
    "sim_dt_s": 0.001,
    "actor_frame_names": [
        "pitch_rad",
        "roll_rad",
        "gyro_x",
        "gyro_y",
        "gyro_z",
        "wheel_left_angle",
        "wheel_right_angle",
        "wheel_left_speed",
        "wheel_right_speed",
        "leg_left_target_est",
        "leg_right_target_est",
        "height_command_rad",
        "roll_command_rad",
        "previous_legL",
        "previous_legR",
        "previous_wheelL",
        "previous_wheelR",
        "reserved17",
        "reserved18",
        "voltageL",
        "voltageR",
    ],
    "actor_scales": [
        0.2,
        0.2,
        5.0,
        5.0,
        5.0,
        10.0,
        10.0,
        30.0,
        30.0,
        0.5,
        0.5,
        0.5,
        0.2,
        1.0,
        1.0,
        1.0,
        1.0,
        30.0,
        30.0,
        7.4,
        7.4,
    ],
    "actor_frames": 3,
    "actor_obs_dim": 63,
    "actor_privileged": [],
    "critic_privileged": [],
    "leg_sensor": "unavailable: target history first-order estimate only; actual servo tracking differs",
    "hardware_deployment_prohibited": True,
    "action_delay_ms": 7,
    "previous_wheels": "last accepted clipped normalized voltage",
    "hardware_wheel_mapping": "q/v/U both wheels negated by firmware adapter",
    "reset": "training cold zero; hardware U seeded from last applied model-domain voltage and previous=U/7.4",
    "leg_update": "target += clip(desired-target,-.003,.003); estimate += .025*(desired-estimate), every 1ms",
    "firmware_delay": "do not add synthetic 7ms on hardware",
}
CONFIG_EXPECTED = {
    "control_dt_s": 0.01,
    "action_delay_ms": [7, 7],
    "wheel_p_hz": 0,
    "wheel_p_schedule": "disabled-direct-voltage",
    "teacher_channels": [0, 1],
}


def validate_contract(contract, config):
    if not isinstance(contract, dict) or not isinstance(config, dict):
        raise ValueError("缺少直压合同或配置")
    for key, value in EXPECTED.items():
        if contract.get(key) != value:
            raise ValueError("直压执行契约不符：" + key)
    if set(contract) != set(EXPECTED):
        raise ValueError("未知合同字段需显式审核")
    # 仅接受本次审核的精确修复配置，其余格式仍走旧断言。
    if config == REPAIR_CONFIG:
        return
    for key, value in CONFIG_EXPECTED.items():
        if config.get(key) != value:
            raise ValueError("训练配置不符：" + key)


def validate_identity(meta, root):
    if (
        meta.get("contract_format") != FORMAT
        or meta.get("format") != "public-dr-stability5-fixed-v1"
    ):
        raise ValueError("当前源码仅接受已迁移的stability5模型合同")
    validate_contract(meta.get("deployment_contract"), meta.get("deployment_config"))
    contract_sha = hashlib.sha256(
        json.dumps(meta["deployment_contract"], sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()
    if contract_sha != meta.get("contract_sha256"):
        raise ValueError("合同指纹不符")
    if hashlib.sha256((root / "model/Actor.h").read_bytes()).hexdigest() != meta.get(
        "actor_sha256"
    ):
        raise ValueError("Actor与元数据不符")
    if meta["sample_id"] != "direct-wheel-voltage-u%d-%s" % (
        meta["iteration"],
        meta["checkpoint_sha256"][:12],
    ):
        raise ValueError("模型样品身份不符")
