const http = require('http');
const os = require('os');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

/**
 * Phone config page, served by the desktop app on the local network.
 * The board shows this URL as a QR code (Settings screen). The URL carries a random
 * key so other people on the Wi-Fi can't stumble onto it.
 *
 *   GET  /c/<key>/                 the page (renderer/config.html)
 *   GET  /c/<key>/cats/<file>.png  cat previews
 *   GET  /c/<key>/api/settings     current settings + status
 *   POST /c/<key>/api/settings     JSON with any settings to change
 */
const FIRST_PORT = 8787;
const RENDERER = path.join(__dirname, '..', 'renderer');

// Private IPv4 of the network adapter the phone is most likely on. Skips virtual
// adapters (Hyper-V, WSL, VirtualBox, VPNs) that Windows PCs often have.
function lanAddress() {
    const skip = /vEthernet|virtual|vmware|vbox|hyper-v|wsl|docker|loopback|tailscale|zerotier|utun|bridge|awdl|llw/i;
    const cands = [];
    for (const [name, addrs] of Object.entries(os.networkInterfaces())) {
        if (skip.test(name)) continue;
        for (const a of addrs || []) {
            if (a.family !== 'IPv4' || a.internal) continue;
            const ip = a.address;
            let rank = 9;
            if (ip.startsWith('192.168.')) rank = 1;
            else if (ip.startsWith('10.')) rank = 2;
            else if (/^172\.(1[6-9]|2\d|3[01])\./.test(ip)) rank = 3;
            else if (ip.startsWith('169.254.')) continue;  // no DHCP address
            if (/wi-?fi|wlan|en0|ethernet|eth0|en1/i.test(name)) rank -= 0.5;
            cands.push({ ip, rank });
        }
    }
    cands.sort((a, b) => a.rank - b.rank);
    return cands.length ? cands[0].ip : '127.0.0.1';
}

class ConfigServer {
    /**
     * @param {object} o
     * @param {() => string} o.getKey      the URL key (stored in settings)
     * @param {() => object} o.getState    settings + status for the page
     * @param {(changes: object) => Promise<object>} o.onChange  apply changes, return new state
     */
    constructor(o) {
        this.o = o;
        this.server = null;
        this.port = 0;
    }

    static newKey() {
        return crypto.randomBytes(4).toString('hex');
    }

    url(host) {
        return `http://${host || lanAddress()}:${this.port}/c/${this.o.getKey()}/`;
    }

    start() {
        return new Promise((resolve) => {
            const tryPort = (port) => {
                const srv = http.createServer((req, res) => this.handle(req, res));
                srv.once('error', (err) => {
                    if (err.code === 'EADDRINUSE' && port < FIRST_PORT + 10) return tryPort(port + 1);
                    console.error('Config server failed to start:', err.message);
                    resolve(false);
                });
                srv.listen(port, '0.0.0.0', () => {
                    this.server = srv;
                    this.port = port;
                    console.log(`📱 Config page: ${this.url()}`);
                    resolve(true);
                });
            };
            tryPort(FIRST_PORT);
        });
    }

    stop() {
        if (this.server) this.server.close();
    }

    send(res, code, type, body) {
        res.writeHead(code, { 'Content-Type': type, 'Cache-Control': 'no-store' });
        res.end(body);
    }

    async handle(req, res) {
        try {
            const u = new URL(req.url, 'http://x');
            const prefix = `/c/${this.o.getKey()}/`;
            if (!u.pathname.startsWith(prefix)) return this.send(res, 404, 'text/plain', 'Not found');
            const rest = u.pathname.slice(prefix.length);

            if (rest === '' && req.method === 'GET') {
                return this.send(res, 200, 'text/html; charset=utf-8', fs.readFileSync(path.join(RENDERER, 'config.html')));
            }
            if (/^cats\/[a-z0-9_]+\.png$/.test(rest) && req.method === 'GET') {
                const file = path.join(RENDERER, rest);
                if (!fs.existsSync(file)) return this.send(res, 404, 'text/plain', 'Not found');
                res.writeHead(200, { 'Content-Type': 'image/png', 'Cache-Control': 'no-cache' });  // art changes with updates
                return res.end(fs.readFileSync(file));
            }
            if (rest === 'fonts/PressStart2P-Regular.ttf' && req.method === 'GET') {
                res.writeHead(200, { 'Content-Type': 'font/ttf', 'Cache-Control': 'max-age=86400' });
                return res.end(fs.readFileSync(path.join(RENDERER, rest)));
            }
            if (rest === 'api/settings' && req.method === 'GET') {
                return this.send(res, 200, 'application/json', JSON.stringify(this.o.getState()));
            }
            if (rest === 'api/settings' && req.method === 'POST') {
                let body = '';
                for await (const chunk of req) {
                    body += chunk;
                    if (body.length > 10000) return this.send(res, 413, 'text/plain', 'Too large');
                }
                const changes = JSON.parse(body || '{}');
                const state = await this.o.onChange(changes);
                return this.send(res, 200, 'application/json', JSON.stringify(state));
            }
            this.send(res, 404, 'text/plain', 'Not found');
        } catch (error) {
            console.error('Config server error:', error.message);
            this.send(res, 400, 'application/json', JSON.stringify({ error: error.message }));
        }
    }
}

module.exports = ConfigServer;
module.exports.lanAddress = lanAddress;
