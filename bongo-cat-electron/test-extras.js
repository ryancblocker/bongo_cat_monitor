// Run: node test-extras.js   (no Electron needed)
const assert = require('assert');
const { parseLhm, parseNvidiaSmi, weatherWord, formatDate, cToUnit } = require('./src/extras-monitor');

const s = (Text, Value) => ({ Text, Value, Children: [] });
const group = (Text, ...Children) => ({ Text, Children });
const hw = (Text, ImageURL, ...Children) => ({ Text, ImageURL, Children });
const tree = (...hardware) => ({ Text: 'Sensor', Children: [hw('DESKTOP-ROSS', 'images_icon/computer.png', ...hardware)] });

// AMD CPU + NVIDIA GPU + Intel iGPU (discrete GPU must win)
let r = parseLhm(tree(
    hw('Gigabyte B550', 'images_icon/mainboard.png', group('Temperatures', s('System', '35.0 °C'))),
    hw('AMD Ryzen 7 5800X', 'images_icon/cpu.png',
        group('Load', s('CPU Total', '12.0 %')),
        group('Temperatures', s('Core (Tctl/Tdie)', '58.4 °C'), s('CCD1 (Tdie)', '55.0 °C'))),
    hw('Intel UHD Graphics', 'images_icon/intel.png', group('Load', s('D3D 3D', '1.0 %'))),
    hw('NVIDIA GeForce RTX 3070', 'images_icon/nvidia.png',
        group('Temperatures', s('GPU Core', '61.0 °C'), s('GPU Hot Spot', '72.1 °C')),
        group('Load', s('GPU Memory Controller', '5.0 %'), s('GPU Core', '34.0 %')))));
assert.deepStrictEqual(r, { cpuTempC: 58.4, gpuLoad: 34, gpuTempC: 61 });

// Intel CPU (CPU Package) + AMD GPU, European decimal comma
r = parseLhm(tree(
    hw('Intel Core i7-12700K', 'images_icon/cpu.png',
        group('Temperatures', s('CPU Core #1', '50,0 °C'), s('CPU Package', '63,5 °C'))),
    hw('AMD Radeon RX 6800', 'images_icon/ati.png',
        group('Load', s('GPU Core', '88,0 %')), group('Temperatures', s('GPU Core', '70,0 °C')))));
assert.deepStrictEqual(r, { cpuTempC: 63.5, gpuLoad: 88, gpuTempC: 70 });

// LHM running without admin: CPU temps missing, nothing crashes
r = parseLhm(tree(hw('AMD Ryzen 5 3600', 'images_icon/cpu.png', group('Load', s('CPU Total', '3 %')))));
assert.deepStrictEqual(r, { cpuTempC: null, gpuLoad: null, gpuTempC: null });
assert.deepStrictEqual(parseLhm({}), { cpuTempC: null, gpuLoad: null, gpuTempC: null });

// nvidia-smi
assert.deepStrictEqual(parseNvidiaSmi('63, 71\r\n'), { gpuLoad: 63, gpuTempC: 71 });
assert.deepStrictEqual(parseNvidiaSmi('[N/A], 45\n'), { gpuLoad: null, gpuTempC: 45 });
assert.strictEqual(parseNvidiaSmi(''), null);

// units and words (row-width limits from the firmware layout)
assert.strictEqual(cToUnit(58.4, 'F'), 137);
assert.strictEqual(cToUnit(58.4, 'C'), 58);
assert.strictEqual(cToUnit(null, 'F'), -1);
for (const code of [0, 1, 2, 3, 45, 48, 51, 55, 61, 65, 71, 75, 80, 85, 95, 99]) {
    const w = weatherWord(code);
    assert.ok(w.length > 0 && w.length <= 7, `code ${code} -> "${w}"`);
}
assert.strictEqual(formatDate(new Date(2026, 10, 12)), 'THU 12');
assert.ok(formatDate(new Date(2026, 8, 30)).length <= 6);

// Full serial lines
const ExtrasMonitor = require('./src/extras-monitor');
const { EventEmitter } = require('events');
const m = new ExtrasMonitor(new EventEmitter());
m.settings = { tempUnit: 'F' };
m.hw = { cpuTempC: 58.4, gpuLoad: 34, gpuTempC: 61 };
m.weather = { tempF: 53.6, code: 3, at: Date.now() };
assert.strictEqual(m.hwCommand(), 'HW:CPUT:137,GPU:34,GPUT:142,U:F');
assert.strictEqual(m.weatherCommand(), 'WX:54F|CLOUDY');
m.settings = { tempUnit: 'F', showGpu: false, showWeather: false, showDate: false };
assert.strictEqual(m.hwCommand(), 'HW:CPUT:137,GPU:-1,GPUT:-1,U:F');
assert.strictEqual(m.weatherCommand(), 'WX:');
assert.strictEqual(m.dateCommand(), 'DATE:');
m.settings = {}; m.hw = { cpuTempC: null, gpuLoad: null, gpuTempC: null };
assert.strictEqual(m.hwCommand(), 'HW:CPUT:-1,GPU:-1,GPUT:-1,U:F');

console.log('extras tests passed');
