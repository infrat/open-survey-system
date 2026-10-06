// OSS RTK Rover web UI - plain JS, no build step.
// Talks to the REST API in firmware/rover/src/web_server.cpp.
(function () {
    'use strict';

    var STATUS_POLL_MS = 3000;
    var STATUS_TIMEOUT_MS = 5000;
    var STATUS_MAX_MISSES = 3;
    var HEARTBEAT_IDLE_MS = 5 * 60 * 1000; // stop holding WiFi on after this long without input
    var RECONNECT_FIRST_MS = 3000; // the rover restarts ~1.5s after answering
    var RECONNECT_POLL_MS = 1500;
    var RECONNECT_TIMEOUT_MS = 45000;
    var SCAN_POLL_MS = 1000;
    var SCAN_TIMEOUT_MS = 20000;
    var RESERVED_PORTS = [80, 23, 3232]; // HTTP, Telnet, OTA

    var TRANSPORT_LABEL = { ble: 'Bluetooth LE', spp: 'Bluetooth SPP', tcp: 'WiFi (TCP)' };
    var WIFI_MODE_LABEL = { ap: 'Access point', sta: 'Station', ap_sta: 'AP + Station' };
    var WIFI_MODE_HINT = {
        ap: 'The rover creates its own network. Connect your phone to it to reach this page.',
        sta: 'The rover joins an existing network. If it cannot connect within 10 seconds, it opens its own access point instead.',
        ap_sta: 'The rover joins an existing network and keeps its own access point available.'
    };

    var config = null; // last config read from the device
    var status = null; // last status read from the device
    var busy = false;  // saving / restarting / waiting for the device
    var statusMisses = 0;
    var statusInFlight = false;
    var lastInteraction = Date.now();
    var scanToken = 0; // invalidates a running scan when the dialog closes

    function $(id) { return document.getElementById(id); }

    function radioValue(name) {
        var el = document.querySelector('input[name="' + name + '"]:checked');
        return el ? el.value : null;
    }

    function setRadio(name, value) {
        var el = document.querySelector('input[name="' + name + '"][value="' + value + '"]');
        if (el) el.checked = true;
    }

    function sleep(ms) {
        return new Promise(function (resolve) { setTimeout(resolve, ms); });
    }

    // ===== API =====

    function api(method, path, body, timeoutMs) {
        var controller = new AbortController();
        var timer = setTimeout(function () { controller.abort(); }, timeoutMs || 8000);
        var options = { method: method, signal: controller.signal, cache: 'no-store' };
        if (body !== undefined) {
            options.headers = { 'Content-Type': 'application/json' };
            options.body = JSON.stringify(body);
        }
        return fetch(path, options)
            .then(function (res) {
                return res.json().catch(function () { return {}; }).then(function (data) {
                    if (!res.ok) {
                        throw new Error(data.message || ('Request failed (' + res.status + ')'));
                    }
                    return data;
                });
            })
            .finally(function () { clearTimeout(timer); });
    }

    // ===== Toasts =====

    function toast(message, type) {
        var el = document.createElement('div');
        el.className = 'toast ' + (type || 'success');
        el.textContent = message;
        $('toasts').appendChild(el);
        setTimeout(function () { el.remove(); }, type === 'error' ? 6000 : 3500);
    }

    // ===== Tabs =====

    function showTab(name) {
        document.querySelectorAll('.tab').forEach(function (tab) {
            var active = tab.dataset.tab === name;
            tab.setAttribute('aria-selected', active ? 'true' : 'false');
            $('panel-' + tab.dataset.tab).hidden = !active;
        });
    }

    // ===== Status =====

    function formatDuration(totalSec) {
        var s = Math.max(0, Math.floor(totalSec));
        var d = Math.floor(s / 86400);
        var h = Math.floor((s % 86400) / 3600);
        var m = Math.floor((s % 3600) / 60);
        var sec = s % 60;
        if (d > 0) return d + 'd ' + h + 'h';
        if (h > 0) return h + 'h ' + m + 'min';
        if (m > 0) return m + 'min ' + sec + 's';
        return sec + 's';
    }

    function setValue(id, text, className) {
        var el = $(id);
        el.textContent = text;
        el.className = className || '';
    }

    function hostList() {
        var hosts = ['ossrtk.local'];
        if (status && status.wifi) {
            if (status.wifi.staIp) hosts.push(status.wifi.staIp);
            if (status.wifi.apIp) hosts.push(status.wifi.apIp);
        }
        return hosts;
    }

    function renderStatus() {
        var online = !!status;
        var pill = $('linkPill');
        $('offlineNote').hidden = online || busy;
        $('setupNote').hidden = !online || status.transport.active;

        if (!online) {
            pill.hidden = false;
            pill.className = 'pill off';
            pill.textContent = busy ? 'Restarting' : 'Offline';
            return;
        }

        var tr = status.transport;
        var w = status.wifi;

        // During the setup window only WiFi runs; the Bluetooth link starts after it
        var label = TRANSPORT_LABEL[tr.mode] || tr.name;
        pill.hidden = false;
        pill.className = 'pill' + (tr.connected ? ' on' : '');
        pill.textContent = !tr.active ? 'Setup mode' : (tr.connected ? 'Client connected' : 'Waiting for client');

        if (!tr.active) {
            $('setupText').textContent = holdingWifi()
                ? label + ' is off while this page is open. It starts when you close the page or press the button.'
                : label + ' starts in ' + formatDuration(w.remainingSec) + ' unless you use this page.';
        }

        setValue('stTransport', label);
        if (!tr.active) {
            setValue('stClient', 'Starts when WiFi turns off', 'dim');
        } else if (tr.connected) {
            setValue('stClient', tr.clients > 1 ? tr.clients + ' connected' : 'Connected', 'ok');
        } else {
            setValue('stClient', 'Not connected', 'dim');
        }
        $('stTcpRow').hidden = tr.mode !== 'tcp';
        if (tr.mode === 'tcp' && config) {
            $('stTcpAddr').textContent = hostList().map(function (h) { return h + ':' + config.tcpPort; }).join(', ');
        }

        if (!w.enabled) {
            setValue('stWifiMode', 'Off', 'dim');
        } else {
            setValue('stWifiMode', WIFI_MODE_LABEL[w.mode] || w.mode);
        }
        if (w.staConnected) {
            setValue('stSta', w.staSsid + '\n' + w.staIp + ' · ' + w.rssi + ' dBm');
        } else {
            setValue('stSta', w.mode === 'ap' ? 'Not used' : 'Not connected', 'dim');
        }
        if (w.apRunning) {
            setValue('stAp', w.apSsid + '\n' + w.apIp + ' · ' +
                w.apClients + (w.apClients === 1 ? ' client' : ' clients'));
        } else {
            setValue('stAp', 'Off', 'dim');
        }
        if (w.remainingSec < 0) {
            setValue('stRemaining', 'Stays on');
        } else {
            setValue('stRemaining', holdingWifi() ? 'When this page is closed' : formatDuration(w.remainingSec));
        }

        setValue('stVersion', status.version);
        setValue('stUptime', formatDuration(status.uptime));
        setValue('stHeap', Math.round(status.freeHeap / 1024) + ' kB');

        updateHints();
    }

    // The rover keeps WiFi up (and Bluetooth off) only while a page is really
    // in use: visible, and touched within the last few minutes. A forgotten tab
    // or a phone that merely joined the WiFi must not block the Bluetooth link.
    function holdingWifi() {
        return !document.hidden && (Date.now() - lastInteraction) < HEARTBEAT_IDLE_MS;
    }

    function pollStatus() {
        if (busy || statusInFlight) return Promise.resolve();
        statusInFlight = true;
        // Only call the rover offline after several misses in a row
        return api('GET', '/api/status' + (holdingWifi() ? '?ka=1' : ''), undefined, STATUS_TIMEOUT_MS)
            .then(function (data) {
                status = data;
                statusMisses = 0;
            })
            .catch(function () {
                if (++statusMisses >= STATUS_MAX_MISSES) status = null;
            })
            .then(function () {
                statusInFlight = false;
                if (!busy) renderStatus();
            });
    }

    function finishSetup() {
        $('finishBtn').disabled = true;
        api('POST', '/api/wifi/off')
            .then(function () {
                status = null;
                statusMisses = STATUS_MAX_MISSES;
                renderStatus();
                toast('WiFi is turning off. Bluetooth starts in a moment.');
            })
            .catch(function (err) { toast('Could not finish setup: ' + err.message, 'error'); })
            .then(function () { $('finishBtn').disabled = false; });
    }

    // ===== Form =====

    function updateHints() {
        var hosts = hostList();
        var port = $('tcpPort').value || (config ? config.tcpPort : '');
        $('tcpHint').textContent = hosts.map(function (h) { return h + ':' + port; }).join(' or ');

        // The running AP name is the automatic one whenever no custom name is saved
        var auto = '';
        if (config && !config.wifi.apSsid && status && status.wifi.apSsid) {
            auto = ' (' + status.wifi.apSsid + ')';
        }
        $('apAutoName').textContent = auto;
    }

    function updateVisibility() {
        var transport = radioValue('transport');
        var mode = radioValue('wifiMode');

        $('tcpFields').hidden = transport !== 'tcp';
        $('staFields').hidden = mode === 'ap';
        $('apFields').hidden = mode === 'sta';
        $('staRequired').hidden = mode !== 'sta';
        $('wifiModeHint').textContent = WIFI_MODE_HINT[mode] || '';

        $('staPassword').disabled = $('staOpen').checked;
        $('apPassword').disabled = $('apOpen').checked;

        var tcp = transport === 'tcp';
        $('windowSec').disabled = tcp;
        $('activeHint').textContent = tcp
            ? 'Not used with the WiFi (TCP) link - WiFi stays on so apps can connect.'
            : 'After power-on the rover runs WiFi only, for setup and firmware updates. If nobody opens ' +
              'this page (or starts an OTA / Telnet session) within this time, WiFi turns off and ' +
              'Bluetooth starts. Restart the rover to get WiFi back.';

        updateHints();
    }

    function fillForm() {
        var w = config.wifi;
        setRadio('transport', config.transport);
        $('tcpPort').value = config.tcpPort;
        setRadio('wifiMode', w.mode);
        $('staSsid').value = w.staSsid;
        $('apSsid').value = w.apSsid;
        $('windowSec').value = w.windowSec;

        // Passwords are write-only on the device: an empty field keeps the saved one
        $('staPassword').value = '';
        $('apPassword').value = '';
        $('staPassword').placeholder = w.staPasswordSet ? 'Saved - leave empty to keep' : 'Network password';
        $('apPassword').placeholder = w.apPasswordSet ? 'Saved - leave empty to keep' : 'At least 8 characters';
        $('staOpen').checked = !w.staPasswordSet;
        $('apOpen').checked = !w.apPasswordSet;

        clearInvalid();
        updateVisibility();
    }

    function clearInvalid() {
        document.querySelectorAll('.input.invalid').forEach(function (el) { el.classList.remove('invalid'); });
    }

    // Shows the field (switching tab if needed) and reports the problem
    function invalid(tab, id, message) {
        showTab(tab);
        var el = $(id);
        el.classList.add('invalid');
        el.focus();
        toast(message, 'error');
        return null;
    }

    function parseIntField(id) {
        var raw = $(id).value.trim();
        return /^\d+$/.test(raw) ? parseInt(raw, 10) : NaN;
    }

    // Builds the POST /api/config body, or returns null after flagging a field.
    // Mirrors RoverSettings::validate() so mistakes are caught before the round trip.
    function readForm() {
        clearInvalid();

        var transport = radioValue('transport');
        var mode = radioValue('wifiMode');
        var body = { transport: transport, wifi: { mode: mode } };

        if (transport === 'tcp') {
            var port = parseIntField('tcpPort');
            if (isNaN(port) || port < 1 || port > 65535) {
                return invalid('connection', 'tcpPort', 'TCP port must be between 1 and 65535');
            }
            if (RESERVED_PORTS.indexOf(port) !== -1) {
                return invalid('connection', 'tcpPort', 'Port ' + port + ' is used by the rover (HTTP, Telnet or OTA)');
            }
            body.tcpPort = port;
        }

        if (mode !== 'ap') {
            var staSsid = $('staSsid').value;
            if (mode === 'sta' && staSsid.length === 0) {
                return invalid('wifi', 'staSsid', 'Station mode needs a network name (SSID)');
            }
            body.wifi.staSsid = staSsid;

            var staPassword = $('staPassword').value;
            if ($('staOpen').checked) {
                body.wifi.staPassword = '';
            } else if (staPassword.length > 0) {
                body.wifi.staPassword = staPassword;
            } else if (staSsid.length > 0 && (!config.wifi.staPasswordSet || staSsid !== config.wifi.staSsid)) {
                return invalid('wifi', 'staPassword', 'Enter the network password or mark the network as open');
            }
        }

        if (mode !== 'sta') {
            body.wifi.apSsid = $('apSsid').value;

            var apPassword = $('apPassword').value;
            if ($('apOpen').checked) {
                body.wifi.apPassword = '';
            } else if (apPassword.length > 0) {
                if (apPassword.length < 8) {
                    return invalid('wifi', 'apPassword', 'Access point password must be at least 8 characters');
                }
                body.wifi.apPassword = apPassword;
            } else if (!config.wifi.apPasswordSet) {
                return invalid('wifi', 'apPassword', 'Enter an access point password or mark it as open');
            }
        }

        if (transport !== 'tcp') {
            var seconds = parseIntField('windowSec');
            if (isNaN(seconds) || seconds < 15 || seconds > 600) {
                return invalid('wifi', 'windowSec', 'Setup window must be between 15 and 600 seconds');
            }
            body.wifi.windowSec = seconds;
        }

        return body;
    }

    function loadConfig(quiet) {
        return api('GET', '/api/config')
            .then(function (data) {
                config = data;
                fillForm();
                if (!quiet) toast('Settings reloaded');
                return true;
            })
            .catch(function (err) {
                toast('Could not load settings: ' + err.message, 'error');
                return false;
            });
    }

    // ===== Save / restart / reconnect =====

    function setBar(state, text) {
        $('barActions').hidden = state !== 'actions';
        $('barBusy').hidden = state !== 'busy';
        $('barLost').hidden = state !== 'lost';
        if (text) $('busyText').textContent = text;
    }

    function setBusy(value, text) {
        busy = value;
        setBar(value ? 'busy' : 'actions', text);
        if (value) {
            status = null;
            renderStatus();
        }
    }

    function waitForDevice() {
        var deadline = Date.now() + RECONNECT_TIMEOUT_MS;

        function attempt() {
            return api('GET', '/api/status', undefined, 2000).catch(function () {
                if (Date.now() >= deadline) return null;
                return sleep(RECONNECT_POLL_MS).then(attempt);
            });
        }

        return sleep(RECONNECT_FIRST_MS).then(attempt);
    }

    function reconnect(successMessage) {
        setBusy(true, 'Reconnecting to device...');
        return waitForDevice().then(function (data) {
            if (!data) {
                busy = false;
                setBar('lost');
                renderStatus();
                return;
            }
            status = data;
            setBusy(false);
            renderStatus();
            return loadConfig(true).then(function () {
                renderStatus(); // the TCP address row depends on the fresh config
                toast(successMessage);
            });
        });
    }

    function save(body) {
        setBusy(true, 'Saving...');
        api('POST', '/api/config', body)
            .then(function () { return reconnect('Configuration saved. Device reconnected.'); })
            .catch(function (err) {
                setBusy(false);
                toast('Failed to save: ' + err.message, 'error');
                pollStatus();
            });
    }

    function restart() {
        setBusy(true, 'Restarting...');
        api('POST', '/api/restart')
            .then(function () { return reconnect('Device restarted.'); })
            .catch(function (err) {
                setBusy(false);
                toast('Failed to restart: ' + err.message, 'error');
                pollStatus();
            });
    }

    // ===== Confirm dialog =====

    var confirmAction = null;

    function confirmDialog(title, text, okLabel, action) {
        $('confirmTitle').textContent = title;
        $('confirmText').textContent = text;
        $('confirmOk').textContent = okLabel;
        confirmAction = action;
        $('confirmDialog').showModal();
    }

    // What the user is about to lose access to, so the restart is not a surprise
    function saveWarning(body) {
        var text = 'This will save the configuration and restart the device. ' +
            'It will be unavailable for a few seconds.';
        var w = config.wifi;
        var wifiChanged = body.wifi.mode !== w.mode ||
            (body.wifi.staSsid !== undefined && body.wifi.staSsid !== w.staSsid) ||
            body.wifi.staPassword !== undefined ||
            (body.wifi.apSsid !== undefined && body.wifi.apSsid !== w.apSsid) ||
            body.wifi.apPassword !== undefined;
        if (wifiChanged) {
            text += ' WiFi settings changed, so you may need to reconnect to the rover on its new network.';
        }
        return text;
    }

    // ===== WiFi scan =====

    function signalBars(rssi) {
        if (rssi >= -50) return '▂▄▆█';
        if (rssi >= -60) return '▂▄▆';
        if (rssi >= -70) return '▂▄';
        return '▂';
    }

    function svgIcon(id) {
        var ns = 'http://www.w3.org/2000/svg';
        var svg = document.createElementNS(ns, 'svg');
        svg.setAttribute('class', 'icon');
        var use = document.createElementNS(ns, 'use');
        use.setAttribute('href', '#' + id);
        svg.appendChild(use);
        return svg;
    }

    function scanMessage(text) {
        var list = $('scanList');
        list.textContent = '';
        var el = document.createElement('div');
        el.className = 'networks-empty';
        el.textContent = text;
        list.appendChild(el);
    }

    function setScanning(value) {
        $('rescanBtn').disabled = value;
        $('rescanBtn').querySelector('.icon').classList.toggle('spin', value);
        $('rescanText').textContent = value ? 'Scanning...' : 'Rescan';
    }

    function pickNetwork(net) {
        $('staSsid').value = net.ssid;
        $('staPassword').value = '';
        $('staOpen').checked = !net.secured;
        $('scanDialog').close();
        updateVisibility();
        if (net.secured) $('staPassword').focus();
    }

    function renderNetworks(networks) {
        // Strongest first; one row per SSID (mesh networks repeat it), hidden ones skipped
        var seen = {};
        var rows = networks
            .slice()
            .sort(function (a, b) { return b.rssi - a.rssi; })
            .filter(function (net) {
                if (!net.ssid || seen[net.ssid]) return false;
                seen[net.ssid] = true;
                return true;
            });

        if (rows.length === 0) {
            scanMessage('No networks found');
            return;
        }

        var list = $('scanList');
        list.textContent = '';
        rows.forEach(function (net) {
            var row = document.createElement('button');
            row.type = 'button';
            row.className = 'network';
            row.appendChild(svgIcon('i-wifi'));

            var main = document.createElement('div');
            main.className = 'network-main';
            var name = document.createElement('div');
            name.className = 'network-name';
            var ssid = document.createElement('span');
            ssid.textContent = net.ssid;
            name.appendChild(ssid);
            if (net.secured) name.appendChild(svgIcon('i-lock'));
            var sub = document.createElement('div');
            sub.className = 'network-sub';
            sub.textContent = 'Ch ' + net.channel + (net.secured ? '' : ' · Open');
            main.appendChild(name);
            main.appendChild(sub);
            row.appendChild(main);

            var signal = document.createElement('div');
            signal.className = 'network-signal';
            var bars = document.createElement('div');
            bars.className = 'network-bars';
            bars.textContent = signalBars(net.rssi);
            var rssi = document.createElement('div');
            rssi.className = 'network-rssi';
            rssi.textContent = net.rssi + ' dBm';
            signal.appendChild(bars);
            signal.appendChild(rssi);
            row.appendChild(signal);

            row.addEventListener('click', function () { pickNetwork(net); });
            list.appendChild(row);
        });
    }

    function scan() {
        var token = ++scanToken;
        var deadline = Date.now() + SCAN_TIMEOUT_MS;
        setScanning(true);
        scanMessage('Scanning...');

        function poll() {
            return api('GET', '/api/wifi/scan').then(function (data) {
                if (token !== scanToken) return null;
                if (!data.scanning) return data.networks || [];
                if (Date.now() >= deadline) throw new Error('Scan timed out. Try again.');
                return sleep(SCAN_POLL_MS).then(poll);
            });
        }

        poll()
            .then(function (networks) {
                if (token !== scanToken || networks === null) return;
                renderNetworks(networks);
            })
            .catch(function (err) {
                if (token !== scanToken) return;
                scanMessage(err.message);
            })
            .then(function () {
                if (token === scanToken) setScanning(false);
            });
    }

    // ===== Wiring =====

    function init() {
        document.querySelectorAll('.tab').forEach(function (tab) {
            tab.addEventListener('click', function () { showTab(tab.dataset.tab); });
        });

        document.querySelectorAll('input[name="transport"], input[name="wifiMode"], #staOpen, #apOpen')
            .forEach(function (el) { el.addEventListener('change', updateVisibility); });
        $('tcpPort').addEventListener('input', updateHints);
        document.querySelectorAll('.input').forEach(function (el) {
            el.addEventListener('input', function () { el.classList.remove('invalid'); });
        });

        $('saveBtn').addEventListener('click', function () {
            if (!config) {
                toast('Settings are not loaded yet', 'error');
                return;
            }
            var body = readForm();
            if (!body) return;
            confirmDialog('Save Configuration?', saveWarning(body), 'Save & Restart', function () { save(body); });
        });

        $('reloadBtn').addEventListener('click', function () {
            loadConfig(false);
            pollStatus();
        });

        $('restartBtn').addEventListener('click', function () {
            confirmDialog('Restart Device?',
                'This will restart the device without saving changes. It will be unavailable for a few seconds.',
                'Restart', restart);
        });

        $('finishBtn').addEventListener('click', finishSetup);
        ['pointerdown', 'keydown', 'input'].forEach(function (type) {
            document.addEventListener(type, function () { lastInteraction = Date.now(); }, true);
        });

        $('retryBtn').addEventListener('click', function () { reconnect('Device reconnected.'); });

        $('confirmCancel').addEventListener('click', function () { $('confirmDialog').close(); });
        $('confirmOk').addEventListener('click', function () {
            $('confirmDialog').close();
            if (confirmAction) confirmAction();
        });

        $('scanBtn').addEventListener('click', function () {
            $('scanDialog').showModal();
            scan();
        });
        $('rescanBtn').addEventListener('click', scan);
        $('scanClose').addEventListener('click', function () { $('scanDialog').close(); });
        $('scanDialog').addEventListener('close', function () {
            scanToken++; // drop the result of a scan still in flight
            setScanning(false);
        });

        // Click on the backdrop closes a dialog
        document.querySelectorAll('dialog').forEach(function (dialog) {
            dialog.addEventListener('click', function (event) {
                if (event.target === dialog) dialog.close();
            });
        });

        loadConfig(true).then(pollStatus);
        setInterval(function () {
            // No polling from a background tab: it would only burn rover battery
            if (!document.hidden) pollStatus();
        }, STATUS_POLL_MS);
        document.addEventListener('visibilitychange', function () {
            if (!document.hidden) pollStatus();
        });
    }

    init();
})();
