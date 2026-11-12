/**
 * Settings the phone page may change, with their allowed values, and the
 * SET: line that carries the board's share of them over USB.
 */
const STYLES = ['hd', '8bit'];  // HD pixel cat, or the original 8-bit sprites
const COATS = ['white', 'black', 'grey', 'orange', 'calico', 'tuxedo', 'siamese', 'cream'];

const RULES = {
    style: v => STYLES.includes(v) ? v : undefined,
    coat: v => COATS.includes(v) ? v : undefined,
    catName: v => typeof v === 'string' ? cleanName(v) : undefined,
    brightness: v => int(v, 10, 255),
    cycleSec: v => int(v, 3, 60),
    showWeatherCard: v => typeof v === 'boolean' ? v : undefined,
    showPcCard: v => typeof v === 'boolean' ? v : undefined,
    timeFormat: v => (v === '12' || v === '24') ? v : undefined,
    sleepTimeout: v => int(v, 1, 60),
    weatherLocation: v => typeof v === 'string' ? v.trim().slice(0, 60) : undefined,
    placeName: v => typeof v === 'string' ? cleanName(v.slice(0, 30)).slice(0, 30) : undefined,
    tempUnit: v => (v === 'F' || v === 'C') ? v : undefined,
    showCpuTemp: v => typeof v === 'boolean' ? v : undefined,
    showGpu: v => typeof v === 'boolean' ? v : undefined,
    startAtLogin: v => typeof v === 'boolean' ? v : undefined,
    focusMin: v => int(v, 5, 120),
};

function int(v, lo, hi) {
    const n = Math.round(Number(v));
    return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : undefined;
}

// The board's fonts cover printable ASCII; '|' separates fields on the wire.
function cleanName(v) {
    return v.normalize('NFKD').replace(/[^\x20-\x7E]/g, '').replace(/\|/g, '').trim().slice(0, 16);
}

/** Keep only known keys with valid values. */
function validate(changes) {
    const out = {};
    for (const [k, rule] of Object.entries(RULES)) {
        if (changes && k in changes) {
            const v = rule(changes[k]);
            if (v !== undefined) out[k] = v;
        }
    }
    return out;
}

/** "SET:style=cartoon|cat=classic|..." from the full settings object. */
function setLine(s) {
    const cards = (s.showWeatherCard !== false ? 1 : 0) | (s.showPcCard !== false ? 2 : 0);
    return 'SET:' + [
        `style=${STYLES.includes(s.style) ? s.style : 'hd'}`,
        `coat=${COATS.includes(s.coat) ? s.coat : 'white'}`,
        `name=${cleanName(s.catName || '')}`,
        `bright=${int(s.brightness, 10, 255) ?? 200}`,
        `cycle=${int(s.cycleSec, 3, 60) ?? 8}`,
        `cards=${cards}`,
        `h24=${s.timeFormat === '24' ? 1 : 0}`,
        `sleep=${int(s.sleepTimeout, 1, 60) ?? 5}`,
        `focus=${int(s.focusMin, 5, 120) ?? 25}`,
    ].join('|');
}

module.exports = { validate, setLine, cleanName, STYLES, COATS };
