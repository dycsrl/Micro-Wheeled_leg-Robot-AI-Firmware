#pragma once
static const char page[] PROGMEM = R"HTML(<!doctype html>
<meta charset="utf-8">
<title>AI站稳实验</title>
<h2>AI站稳实验 .02</h2>
<p>未标定候选；请使用机械保护。关闭页面后板端1500ms心跳失联时，运行中的AI交回原生；原生并不自动停机，后台标签页也可能暂停心跳。日志TCP 9871独立。</p>
<button id="claim" onclick="claim()">取得控制会话</button>
<span id="owner">未取得控制权限</span>
<p id="notice" role="status">
</p>
<p id="heartbeatNotice" role="status">
</p>
<div id="controls">
<button onclick="cmd(1)" disabled>原生站立</button>
<button onclick="cmd(2)" disabled>AI</button>
<button onclick="cmd(3)" disabled>回原生</button>
<button onclick="cmd(0)" disabled>停止/收腿</button>
<button onclick="cmd(4)" disabled>舵机失能</button>
</div>
<pre id="s">尚无新鲜状态</pre><script>
let token = '',
    lastFresh = -Infinity,
    claimBusy = false,
    commandBusy = false,
    heartbeatConfirmed = false;
const statusBox = document.getElementById('s'),
    notice = document.getElementById('notice');
const modes = ['已停止', '原生控制', 'AI控制', '保护停机', '舵机失能'],
    reasons = ['无', '上电', '人工停止', '控制心跳失联', '非有限数据', '倾角保护', '低电保护', '控制超时', '模型失败', '未满足站立条件', '主动回原生', '垂直冲击保护'];

function ownership(message) {
    document.getElementById('owner').textContent = token && !heartbeatConfirmed ? '控制心跳未确认' : message;
    document.getElementById('claim').disabled = !!token || claimBusy;
    document.querySelectorAll('#controls button').forEach(b => b.disabled = !token || !heartbeatConfirmed || commandBusy);
}

function lost(message) {
    token = '';
    heartbeatConfirmed = false;
    document.getElementById('heartbeatNotice').textContent = '';
    ownership('无控制权限');
    notice.textContent = message + '；请重新取得控制会话。未确认机器人实际停止。';
}
async function request(url, options = {}, timeout = 600) {
    const abort = new AbortController(),
        timer = setTimeout(() => abort.abort(), timeout);
    try {
        const r = await fetch(url, {
            ...options,
            cache: 'no-store',
            signal: abort.signal
        });
        const text = await r.text();
        if (!r.ok) {
            const e = Error(text);
            e.status = r.status;
            throw e;
        }
        return text;
    } finally {
        clearTimeout(timer);
    }
}

function post(t, extra = '') {
    return {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: 'token=' + encodeURIComponent(t) + extra
    };
}
async function claim() {
    if (claimBusy || token) return;
    claimBusy = true;
    ownership('正在申请控制权限');
    try {
        token = await request('/claim', {
            method: 'POST'
        });
        heartbeatConfirmed = true;
        notice.textContent = '已取得权限；动作须手动点击。';
    } catch (e) {
        lost(e.status === 409 ? '已有其他控制会话' : '取得控制会话失败');
    } finally {
        claimBusy = false;
        ownership(token ? '控制权限有效' : '无控制权限');
    }
}
async function cmd(c) {
    if (!token || !heartbeatConfirmed || commandBusy) return;
    const t = token;
    commandBusy = true;
    ownership('控制权限有效');
    try {
        await request('/command', post(t, '&code=' + c));
        notice.textContent = '命令已入队，执行结果以新鲜状态为准。';
    } catch (e) {
        if (t === token && e.status === 403) lost('控制会话已失效');
        else notice.textContent = '命令未确认：网络超时或请求被拒绝，请查看实际状态。';
    } finally {
        commandBusy = false;
        ownership(token ? '控制权限有效' : '无控制权限');
    }
}
// 两条独立单飞调度链；状态慢响应不再阻塞心跳，失败不自动抢回权限。
async function heartbeat() {
    const t = token;
    try {
        if (t) {
            await request('/heartbeat', post(t));
            if (t === token) {
                heartbeatConfirmed = true;
                document.getElementById('heartbeatNotice').textContent = '';
                ownership('控制权限有效');
            }
        }
    } catch (e) {
        if (t && t === token) {
            if (e.status === 403) lost('控制会话已失效');
            else {
                heartbeatConfirmed = false;
                document.getElementById('heartbeatNotice').textContent = '控制心跳未确认，正在重试；未确认控制权限，板端1500ms失联保护保持不变。';
                ownership('控制心跳未确认');
            }
        }
    } finally {
        setTimeout(heartbeat, 200);
    }
}
async function poll() {
    const started = performance.now();
    try {
        const j = JSON.parse(await request('/status'));
        const age = Number(j.age_ms) + performance.now() - started;
        if (!Number.isFinite(age) || age >= 1000) throw Error('stale');
        lastFresh = performance.now() - age;
        statusBox.textContent = '连接：状态可读（不代表控制权限）\n模式：' + (modes[j.mode] ?? '未知') + '\n原因：' + (reasons[j.reason] ?? '未知') + '\n电池：' + j.voltage + ' V / 估算 ' + j.percent_estimate + '%' + (j.reason === 6 ? '（低电保护）' : '') + '\n固件：' + j.firmware + '\n模型：' + (j.model_identity ? j.model_identity.generation + ' / seed ' + j.model_identity.seed + ' / 更新 ' + j.model_identity.iteration + ' / ' + (j.model_identity.new_step == null ? 'step ' + j.model_identity.step : '新增 ' + j.model_identity.new_step) + ' / 来源step ' + j.model_identity.source_step : '未知');
        statusBox.style.color = 'black';
    } catch (e) {
        lastFresh = -Infinity;
        statusBox.textContent = '连接中断或状态过期，实际模式未知';
        statusBox.style.color = 'red';
    } finally {
        setTimeout(poll, 250);
    }
}
setInterval(() => {
    if (performance.now() - lastFresh > 1000) {
        statusBox.textContent = '状态过期，实际模式未知（控制权限另列）';
        statusBox.style.color = 'red';
    }
}, 100);
heartbeat();
poll();
</script>)HTML";
