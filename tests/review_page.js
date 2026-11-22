// 抽取真实HTML脚本，离线执行实际事件函数；不会连接机器人。
const fs = require('fs'),
    vm = require('vm'),
    assert = require('assert'),
    path = require('path');
const html = fs.readFileSync(path.join(__dirname, '../src/network/Page.h'), 'utf8');
const code = html.split('<script>')[1].split('</script>')[0];

function harness() {
    let now = 0,
        id = 0,
        active = 0,
        maxActive = 0;
    const timers = new Map(),
        calls = [],
        buttons = Array.from({
            length: 5
        }, () => ({
            disabled: true
        }));
    const elements = Object.fromEntries([...html.matchAll(/id="([^"]+)"/g)].map(x => [x[1], {
        textContent: '',
        style: {},
        disabled: false
    }]));
    const plans = [];

    function timer(fn, ms) {
        timers.set(++id, {
            fn,
            due: now + ms
        });
        return id;
    }
    const context = {
        document: {
            getElementById: id => {
                assert(elements[id], id);
                return elements[id];
            },
            querySelectorAll: () => buttons
        },
        performance: {
            now: () => now
        },
        AbortController,
        setTimeout: timer,
        clearTimeout: i => timers.delete(i),
        setInterval: () => 0,
        fetch: (url, opts) => {
            calls.push({
                url,
                body: opts.body,
                at: now
            });
            if (url === '/status') return new Promise(() => {});
            if (url === '/claim') return Promise.resolve({
                ok: true,
                text: async () => 'new'
            });
            if (url === '/command') return Promise.resolve({
                ok: true,
                text: async () => ''
            });
            assert.equal(url, '/heartbeat');
            active++;
            maxActive = Math.max(maxActive, active);
            const plan = plans.shift() || {
                status: 200
            };
            return new Promise((resolve, reject) => {
                let done = false;

                function finish(error) {
                    if (done) return;
                    done = true;
                    active--;
                    if (error) reject(error);
                    else resolve({
                        ok: plan.status === 200,
                        status: plan.status,
                        text: async () => ''
                    });
                }
                opts.signal.addEventListener('abort', () => finish(Error('timeout')));
                if (plan.defer) plan.finish = finish;
                else if (!plan.timeout) Promise.resolve().then(() => finish(plan.error ? Error('network') : null));
            });
        }
    };
    vm.createContext(context);
    vm.runInContext(code, context);
    const flush = async () => {
        for (let i = 0; i < 20; i++) await Promise.resolve();
    };
    async function advance(ms) {
        const end = now + ms;
        await flush();
        while (true) {
            const next = [...timers].sort((a, b) => a[1].due - b[1].due)[0];
            if (!next || next[1].due > end) break;
            now = next[1].due;
            timers.delete(next[0]);
            next[1].fn();
            await flush();
        }
        now = end;
        await flush();
    }
    return {
        context,
        elements,
        buttons,
        plans,
        calls,
        advance,
        flush,
        eval: s => vm.runInContext(s, context),
        max: () => maxActive
    };
}
(async () => {
    const passed = [];
    {
        const h = harness();
        await h.context.claim();
        h.plans.push({
            timeout: true
        }, {
            status: 200
        });
        await h.advance(800);
        assert.equal(h.eval('token'), 'new');
        assert.equal(h.eval('heartbeatConfirmed'), false);
        assert(h.buttons.every(b => b.disabled));
        assert(h.elements.heartbeatNotice.textContent.includes('正在重试'));
        await h.advance(200);
        assert.equal(h.eval('token'), 'new');
        assert(h.eval('heartbeatConfirmed'));
        assert(h.buttons.every(b => !b.disabled));
        assert.equal(h.elements.heartbeatNotice.textContent, '');
        assert.equal(h.elements.notice.textContent, '已取得权限；动作须手动点击。');
        const beats = h.calls.filter(c => c.url === '/heartbeat');
        assert.equal(beats.length, 2);
        assert(beats.every(c => c.body === 'token=new'));
        assert.equal(h.max(), 1);
        passed.push('一次真实Abort超时后同token自动重试成功并恢复禁控');
    } {
        const h = harness();
        await h.context.claim();
        h.plans.push({
            timeout: true
        }, {
            error: true
        }, {
            status: 503
        }, {
            timeout: true
        }, {
            status: 500
        }, {
            error: true
        });
        await h.advance(2400);
        assert.equal(h.eval('token'), 'new');
        assert(h.buttons.every(b => b.disabled));
        assert.equal(h.max(), 1);
        const beats = h.calls.filter(c => c.url === '/heartbeat');
        assert(beats.length >= 4 && beats.length <= 6);
        assert.equal(h.calls.filter(c => c.url === '/claim').length, 1);
        await h.context.cmd(2);
        assert.equal(h.calls.filter(c => c.url === '/command').length, 0);
        passed.push('连续超时/网络/503/500有限单飞重试，不自动claim或动作');
    } {
        const h = harness();
        await h.context.claim();
        h.plans.push({
            status: 403
        });
        await h.advance(200);
        assert.equal(h.eval('token'), '');
        assert(h.buttons.every(b => b.disabled));
        await h.advance(1000);
        assert.equal(h.calls.filter(c => c.url === '/heartbeat').length, 1);
        assert.equal(h.calls.filter(c => c.url === '/claim').length, 1);
        passed.push('明确403清token禁控，不抢权');
    }
    for (const old of [{
            status: 403,
            defer: true
        }, {
            status: 200,
            defer: true
        }, {
            error: true,
            defer: true
        }]) {
        const h = harness();
        await h.context.claim();
        h.plans.push(old);
        await h.advance(200);
        h.context.lost('旧会话结束');
        await h.context.claim();
        h.eval("token='replacement';heartbeatConfirmed=false;document.getElementById('heartbeatNotice').textContent='新会话正在重试';ownership('控制心跳未确认')");
        old.finish(old.error ? Error('old network') : null);
        await h.flush();
        assert.equal(h.eval('token'), 'replacement');
        assert.equal(h.eval('heartbeatConfirmed'), false);
        assert.equal(h.elements.heartbeatNotice.textContent, '新会话正在重试');
        assert(h.buttons.every(b => b.disabled));
    }
    passed.push('旧token迟到成功/403/网络失败均不影响新会话');
    {
        const h = harness();
        await h.context.claim();
        await h.advance(1400);
        assert(h.calls.filter(c => c.url === '/heartbeat').length >= 6);
        assert.equal(h.calls.filter(c => c.url === '/status').length, 1);
        await h.context.cmd(3);
        const result = h.elements.notice.textContent;
        await h.advance(200);
        assert.equal(h.elements.notice.textContent, result);
        assert.equal(h.max(), 1);
        passed.push('慢status不阻塞心跳，心跳成功不覆盖命令结果');
    }
    console.log(JSON.stringify({
        ok: true,
        passed,
        count: passed.length
    }, null, 2));
})().catch(e => {
    console.error(e);
    process.exitCode = 1;
});
