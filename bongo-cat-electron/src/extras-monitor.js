const { execFile } = require('child_process');
const si = require('systeminformation');

/**
 * Extras for the Ross edition: CPU temperature, GPU load + temperature,
 * date and weather. Produces the serial lines the firmware understands:
 *
 *   HW:CPUT:136,GPU:63,GPUT:160,U:F    (-1 = unknown, row hidden / temp left off)
 *   DATE:THU 12                         (empty = hidden)
 *   WX:54F|CLOUDY                       (temperature | condition; empty = hidden)
 *
 * Where readings come from, in order:
 *   GPU load/temp : nvidia-smi (NVIDIA driver, no setup) -> LibreHardwareMonitor
 *   CPU temp      : LibreHardwareMonitor -> systeminformation
 *   Weather       : Open-Meteo (free, no key). US ZIP -> lat/lon via zippopotam.us
 *
 * LibreHardwareMonitor: Options -> Remote Web Server -> Run (port 8085).
 * Windows doesn't expose CPU temperature to normal apps, so on Windows that's
 * the only reliable source.
 */

const LHM_URL = 'http://localhost:8085/data.json';

// WMO weather codes -> at most 7 characters (row 2 is "WED 30" + condition)
function weatherWord(code) {
    if (code === 0 || code === 1) return 'CLEAR';
    if (code === 2 || code === 3) return 'CLOUDY';
    if (code === 45 || code === 48) return 'FOG';
    if (code >= 51 && code <= 57) return 'DRIZZLE';
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return 'RAIN';
    if ((code >= 71 && code <= 77) || code === 85 || code === 86) return 'SNOW';
    if (code >= 95) return 'STORM';
    return '';
}

function cToUnit(c, unit) {
    if (c === null || c === undefined || !isFinite(c)) return -1;
    return Math.round(unit === 'F' ? c * 9 / 5 + 32 : c);
}

// "58,5 °C" / "12.3 %" -> 58.5 / 12.3
function parseLhmValue(v) {
    if (typeof v === 'number') return v;
    if (typeof v !== 'string') return NaN;
    const m = v.replace(',', '.').match(/-?\d+(\.\d+)?/);
    return m ? parseFloat(m[0]) : NaN;
}

/**
 * Pull CPU temp and GPU load/temp out of LibreHardwareMonitor's data.json tree.
 * Hardware nodes are recognised by their icon (ImageURL), sensors by their
 * group ("Temperatures", "Load") and name. Exported for tests.
 */
function parseLhm(root) {
    const hardware = [];
    (function walk(node) {
        if (!node || typeof node !== 'object') return;
        const img = (node.ImageURL || '').toLowerCase();
        if (/(cpu|nvidia|ati|amd|intel)\.png/.test(img) || /images_icon\/(cpu|nvidia|ati|amd|intel)/.test(img)) {
            hardware.push(node);
            return;
        }
        (node.Children || []).forEach(walk);
    })(root);

    const sensors = (hw, group) => {
        const g = (hw.Children || []).find(c => (c.Text || '').toLowerCase() === group);
        return g ? (g.Children || []).map(s => ({ name: s.Text || '', value: parseLhmValue(s.Value) })).filter(s => isFinite(s.value)) : [];
    };
    const pick = (list, patterns) => {
        for (const p of patterns) {
            const hit = list.find(s => p.test(s.name));
            if (hit) return hit.value;
        }
        return null;
    };

    const result = { cpuTempC: null, gpuLoad: null, gpuTempC: null };

    const cpu = hardware.find(h => /cpu/.test((h.ImageURL || '').toLowerCase()));
    if (cpu) {
        result.cpuTempC = pick(sensors(cpu, 'temperatures'),
            [/package/i, /tctl|tdie/i, /core average/i, /core max/i, /^cpu/i, /core/i]);
    }

    // Prefer a discrete GPU (NVIDIA/AMD) over Intel integrated graphics
    const gpus = hardware.filter(h => !/cpu/.test((h.ImageURL || '').toLowerCase()));
    gpus.sort((a, b) => (/intel/.test((a.ImageURL || '').toLowerCase()) ? 1 : 0) - (/intel/.test((b.ImageURL || '').toLowerCase()) ? 1 : 0));
    const gpu = gpus[0];
    if (gpu) {
        result.gpuLoad = pick(sensors(gpu, 'load'), [/gpu core/i, /^d3d 3d/i, /core/i]);
        result.gpuTempC = pick(sensors(gpu, 'temperatures'), [/gpu core/i, /hot spot/i, /core/i]);
    }
    return result;
}

// "63, 71" -> { gpuLoad: 63, gpuTempC: 71 }. Exported for tests.
function parseNvidiaSmi(stdout) {
    const line = (stdout || '').trim().split(/\r?\n/)[0] || '';
    const [load, temp] = line.split(',').map(s => parseFloat(s.trim()));
    if (!isFinite(load) && !isFinite(temp)) return null;
    return { gpuLoad: isFinite(load) ? load : null, gpuTempC: isFinite(temp) ? temp : null };
}

function formatDate(d = new Date()) {
    const day = d.toLocaleDateString('en-US', { weekday: 'short' }).toUpperCase().slice(0, 3);
    return `${day} ${d.getDate()}`;
}

class ExtrasMonitor {
    constructor(eventEmitter) {
        this.eventEmitter = eventEmitter;
        this.settings = {};
        this.hw = { cpuTempC: null, gpuLoad: null, gpuTempC: null, source: 'none' };
        this.weather = null;          // { tempF, code, at }
        this.place = null;            // { query, lat, lon, name }
        this.weatherError = '';
        this.hwTimer = null;
        this.wxTimer = null;
        this.nvidiaSmiMissing = false;
    }

    updateSettings(settings) {
        const before = this.settings.weatherLocation;
        this.settings = { ...this.settings, ...settings };
        if (this.settings.weatherLocation !== before) {
            this.place = null;
            this.weather = null;
            this.refreshWeather();
        }
    }

    start(settings) {
        this.updateSettings(settings || {});
        this.refreshHardware();
        this.hwTimer = setInterval(() => this.refreshHardware(), 2000);
        this.wxTimer = setInterval(() => this.refreshWeather(), 15 * 60 * 1000);
    }

    stop() {
        clearInterval(this.hwTimer);
        clearInterval(this.wxTimer);
    }

    // ---------- hardware ----------

    nvidiaSmi() {
        if (this.nvidiaSmiMissing) return Promise.resolve(null);
        return new Promise(resolve => {
            execFile('nvidia-smi', ['--query-gpu=utilization.gpu,temperature.gpu', '--format=csv,noheader,nounits'],
                { timeout: 1500, windowsHide: true }, (err, stdout) => {
                    if (err) {
                        if (err.code === 'ENOENT') this.nvidiaSmiMissing = true;
                        return resolve(null);
                    }
                    resolve(parseNvidiaSmi(stdout));
                });
        });
    }

    async lhm() {
        try {
            const res = await fetch(LHM_URL, { signal: AbortSignal.timeout(1000) });
            if (!res.ok) return null;
            return parseLhm(await res.json());
        } catch {
            return null;
        }
    }

    async refreshHardware() {
        if (process.env.BONGO_FAKE_HW) {
            // Testing on a machine without sensors (e.g. a Mac): wobble some plausible numbers
            const w = Math.sin(Date.now() / 5000);
            this.hw = { cpuTempC: 55 + 5 * w, gpuLoad: 60 + 30 * w, gpuTempC: 65 + 6 * w, source: 'fake (BONGO_FAKE_HW)' };
            this.eventEmitter.emit('extras-stats', this.getStatus());
            return;
        }
        const [nv, lhm] = await Promise.all([this.nvidiaSmi(), this.lhm()]);
        let cpuTempC = lhm ? lhm.cpuTempC : null;
        // systeminformation fallback (Mac/Linux). Skipped on Windows, where it
        // launches PowerShell every call and rarely returns a real reading.
        if (cpuTempC === null && process.platform !== 'win32' && !this.siTempMissing) {
            try {
                const t = await si.cpuTemperature();
                if (t && isFinite(t.main) && t.main > 0) cpuTempC = t.main;
                else this.siTempMissing = true;
            } catch { this.siTempMissing = true; }
        }
        const gpuLoad = nv && nv.gpuLoad !== null ? nv.gpuLoad : (lhm ? lhm.gpuLoad : null);
        const gpuTempC = nv && nv.gpuTempC !== null ? nv.gpuTempC : (lhm ? lhm.gpuTempC : null);
        this.hw = {
            cpuTempC, gpuLoad, gpuTempC,
            source: [nv && 'nvidia-smi', lhm && 'LibreHardwareMonitor'].filter(Boolean).join(' + ') || 'none'
        };
        this.eventEmitter.emit('extras-stats', this.getStatus());
    }

    // ---------- weather ----------

    async geocode(query) {
        const q = (query || '').trim();
        if (!q) return null;
        if (/^\d{5}$/.test(q)) {
            const res = await fetch(`https://api.zippopotam.us/us/${q}`, { signal: AbortSignal.timeout(8000) });
            if (!res.ok) throw new Error(`ZIP ${q} not found`);
            const data = await res.json();
            const p = data.places && data.places[0];
            if (!p) throw new Error(`ZIP ${q} not found`);
            return { query: q, lat: parseFloat(p.latitude), lon: parseFloat(p.longitude), name: `${p['place name']}, ${p['state abbreviation']}` };
        }
        // "City" or "City, ST" / "City, State" / "City, Country"
        const [city, region] = q.split(',').map(s => s.trim());
        const res = await fetch(`https://geocoding-api.open-meteo.com/v1/search?count=10&language=en&format=json&name=${encodeURIComponent(city)}`,
            { signal: AbortSignal.timeout(8000) });
        const data = await res.json();
        let results = data.results || [];
        if (region) {
            // "CO" and "Colorado" both match admin1 "Colorado"; "US"/"Canada" match the country
            const want = [region, US_STATES[region.toUpperCase()]].filter(Boolean).map(v => v.toLowerCase());
            const narrowed = results.filter(x =>
                [x.admin1, x.country, x.country_code].filter(Boolean).some(v => want.includes(v.toLowerCase())));
            if (narrowed.length) results = narrowed;
        }
        const p = results[0];
        if (!p) throw new Error(`Couldn't find "${q}"`);
        return { query: q, lat: p.latitude, lon: p.longitude, name: [p.name, p.admin1].filter(Boolean).join(', ') };
    }

    async refreshWeather() {
        const query = (this.settings.weatherLocation || '').trim();
        if (!query) {
            this.weather = null;
            this.weatherError = '';
            this.eventEmitter.emit('extras-stats', this.getStatus());
            return;
        }
        try {
            if (!this.place || this.place.query !== query) this.place = await this.geocode(query);
            const { lat, lon } = this.place;
            const url = `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lon}` +
                `&current=temperature_2m,weather_code&temperature_unit=fahrenheit&timezone=auto`;
            const res = await fetch(url, { signal: AbortSignal.timeout(8000) });
            if (!res.ok) throw new Error(`weather service returned ${res.status}`);
            const data = await res.json();
            this.weather = { tempF: data.current.temperature_2m, code: data.current.weather_code, at: Date.now() };
            this.weatherError = '';
        } catch (error) {
            console.error('Weather update failed:', error.message);
            this.weatherError = error.message;
            // keep the last good reading for up to 2 hours
            if (this.weather && Date.now() - this.weather.at > 2 * 60 * 60 * 1000) this.weather = null;
            // retry sooner than the normal 15 minutes
            clearTimeout(this.retryTimer);
            this.retryTimer = setTimeout(() => this.refreshWeather(), 60 * 1000);
        }
        this.eventEmitter.emit('extras-stats', this.getStatus());
    }

    // ---------- serial lines ----------

    unit() {
        return this.settings.tempUnit === 'C' ? 'C' : 'F';
    }

    hwCommand() {
        const s = this.settings;
        const u = this.unit();
        const cput = s.showCpuTemp === false ? -1 : cToUnit(this.hw.cpuTempC, u);
        const gpu = s.showGpu === false || this.hw.gpuLoad === null ? -1 : Math.round(this.hw.gpuLoad);
        const gput = s.showGpu === false ? -1 : cToUnit(this.hw.gpuTempC, u);
        return `HW:CPUT:${cput},GPU:${gpu},GPUT:${gput},U:${u}`;
    }

    dateCommand() {
        return this.settings.showDate === false ? 'DATE:' : `DATE:${formatDate()}`;
    }

    weatherCommand() {
        if (this.settings.showWeather === false || !this.weather) return 'WX:';
        const u = this.unit();
        const t = u === 'F' ? Math.round(this.weather.tempF) : Math.round((this.weather.tempF - 32) * 5 / 9);
        return `WX:${t}${u}|${weatherWord(this.weather.code)}`;
    }

    getStatus() {
        return {
            hw: this.hw,
            unit: this.unit(),
            weather: this.weather,
            place: this.place ? this.place.name : '',
            weatherError: this.weatherError,
            lines: [this.hwCommand(), this.dateCommand(), this.weatherCommand()]
        };
    }
}

const US_STATES = {
    AL: 'Alabama', AK: 'Alaska', AZ: 'Arizona', AR: 'Arkansas', CA: 'California', CO: 'Colorado', CT: 'Connecticut',
    DE: 'Delaware', FL: 'Florida', GA: 'Georgia', HI: 'Hawaii', ID: 'Idaho', IL: 'Illinois', IN: 'Indiana', IA: 'Iowa',
    KS: 'Kansas', KY: 'Kentucky', LA: 'Louisiana', ME: 'Maine', MD: 'Maryland', MA: 'Massachusetts', MI: 'Michigan',
    MN: 'Minnesota', MS: 'Mississippi', MO: 'Missouri', MT: 'Montana', NE: 'Nebraska', NV: 'Nevada', NH: 'New Hampshire',
    NJ: 'New Jersey', NM: 'New Mexico', NY: 'New York', NC: 'North Carolina', ND: 'North Dakota', OH: 'Ohio',
    OK: 'Oklahoma', OR: 'Oregon', PA: 'Pennsylvania', RI: 'Rhode Island', SC: 'South Carolina', SD: 'South Dakota',
    TN: 'Tennessee', TX: 'Texas', UT: 'Utah', VT: 'Vermont', VA: 'Virginia', WA: 'Washington', WV: 'West Virginia',
    WI: 'Wisconsin', WY: 'Wyoming', DC: 'District of Columbia'
};

module.exports = ExtrasMonitor;
module.exports.parseLhm = parseLhm;
module.exports.parseNvidiaSmi = parseNvidiaSmi;
module.exports.weatherWord = weatherWord;
module.exports.formatDate = formatDate;
module.exports.cToUnit = cToUnit;
