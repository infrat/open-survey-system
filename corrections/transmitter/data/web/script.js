// Tab switching
document.querySelectorAll(".tab-button").forEach((button) => {
  button.addEventListener("click", () => {
    const tabName = button.dataset.tab;

    // Update active tab button
    document
      .querySelectorAll(".tab-button")
      .forEach((btn) => btn.classList.remove("active"));
    button.classList.add("active");

    // Update active tab content
    document
      .querySelectorAll(".tab-content")
      .forEach((content) => content.classList.remove("active"));
    document.getElementById(`${tabName}-tab`).classList.add("active");
  });
});

// Status message helpers
function showStatus(message, isError = false) {
  const statusEl = document.getElementById("status");
  statusEl.textContent = message;
  statusEl.classList.remove("hidden", "success", "error");
  statusEl.classList.add(isError ? "error" : "success");

  setTimeout(() => {
    statusEl.classList.add("hidden");
  }, 5000);
}

// Load configuration from device
async function loadConfig() {
  try {
    const response = await fetch("/api/config");
    if (!response.ok) throw new Error("Failed to load configuration");

    const config = await response.json();
    populateForm(config);
    showStatus("Configuration loaded successfully");
  } catch (error) {
    console.error("Error loading config:", error);
    showStatus("Failed to load configuration", true);
  }
}

// Populate form with configuration
function populateForm(config) {
  // WiFi
  document.getElementById("wifiSSID").value = config.wifi.ssid || "";
  document.getElementById("wifiPassword").value = config.wifi.password || "";
  document.getElementById("wifiTimeout").value = config.wifi.timeout || 30000;

  // NTRIP
  document.getElementById("ntripHost").value = config.ntrip.host || "";
  document.getElementById("ntripPort").value = config.ntrip.port || 2101;
  document.getElementById("ntripMountpoint").value =
    config.ntrip.mountpoint || "";
  document.getElementById("ntripUser").value = config.ntrip.user || "";
  document.getElementById("ntripPassword").value = config.ntrip.password || "";
  document.getElementById("ntripTimeout").value = config.ntrip.timeout || 10000;

  // GGA
  document.getElementById("ggaLatitude").value = config.gga.latitude || 0;
  document.getElementById("ggaLongitude").value = config.gga.longitude || 0;
  document.getElementById("ggaAltitude").value = config.gga.altitude || 0;
  document.getElementById("ggaSendInterval").value =
    config.gga.sendInterval || 2000;

  // LoRa (frequency stored as Hz, displayed as MHz)
  document.getElementById("loraFrequency").value = (
    (config.lora.frequency || 433000000) / 1e6
  )
    .toFixed(6)
    .replace(/\.?0+$/, "");
  document.getElementById("loraSpreadingFactor").value =
    config.lora.spreadingFactor || 7;
  document.getElementById("loraBandwidth").value =
    config.lora.bandwidth || 125000;
  document.getElementById("loraCodingRate").value = config.lora.codingRate || 5;
  document.getElementById("loraTxPower").value = config.lora.txPower || 20;
  document.getElementById("loraSyncWord").value =
    "0x" +
    (config.lora.syncWord || 0x12).toString(16).toUpperCase().padStart(2, "0");

  // RTCM
  document.getElementById("rtcmMessageTypes").value = (
    config.rtcm.messageTypes || []
  ).join(", ");
  document.getElementById("rtcmPriorityTypes").value = (
    config.rtcm.priorityMessageTypes || []
  ).join(", ");
}

// Parse form data to configuration object
function parseFormData() {
  const config = {
    wifi: {
      ssid: document.getElementById("wifiSSID").value,
      password: document.getElementById("wifiPassword").value,
      timeout: parseInt(document.getElementById("wifiTimeout").value),
    },
    ntrip: {
      host: document.getElementById("ntripHost").value,
      port: parseInt(document.getElementById("ntripPort").value),
      mountpoint: document.getElementById("ntripMountpoint").value,
      user: document.getElementById("ntripUser").value,
      password: document.getElementById("ntripPassword").value,
      timeout: parseInt(document.getElementById("ntripTimeout").value),
    },
    gga: {
      latitude: parseFloat(document.getElementById("ggaLatitude").value),
      longitude: parseFloat(document.getElementById("ggaLongitude").value),
      altitude: parseFloat(document.getElementById("ggaAltitude").value),
      sendInterval: parseInt(document.getElementById("ggaSendInterval").value),
    },
    lora: {
      frequency: Math.round(
        parseFloat(
          document.getElementById("loraFrequency").value.replace(",", "."),
        ) * 1e6,
      ),
      spreadingFactor: parseInt(
        document.getElementById("loraSpreadingFactor").value,
      ),
      bandwidth: parseInt(document.getElementById("loraBandwidth").value),
      codingRate: parseInt(document.getElementById("loraCodingRate").value),
      txPower: parseInt(document.getElementById("loraTxPower").value),
      syncWord: parseInt(document.getElementById("loraSyncWord").value, 16),
    },
    rtcm: {
      messageTypes: document
        .getElementById("rtcmMessageTypes")
        .value.split(",")
        .map((s) => parseInt(s.trim()))
        .filter((n) => !isNaN(n)),
      priorityMessageTypes: document
        .getElementById("rtcmPriorityTypes")
        .value.split(",")
        .map((s) => parseInt(s.trim()))
        .filter((n) => !isNaN(n)),
    },
  };

  return config;
}

// Validate configuration
function validateConfig(config) {
  const errors = [];

  // WiFi validation
  if (!config.wifi.ssid) {
    errors.push("WiFi SSID is required");
  }

  // NTRIP validation
  if (!config.ntrip.host) {
    errors.push("NTRIP host is required");
  }
  if (config.ntrip.port < 1 || config.ntrip.port > 65535) {
    errors.push("NTRIP port must be between 1 and 65535");
  }

  // GGA validation
  if (config.gga.latitude < -90 || config.gga.latitude > 90) {
    errors.push("Latitude must be between -90 and 90");
  }
  if (config.gga.longitude < -180 || config.gga.longitude > 180) {
    errors.push("Longitude must be between -180 and 180");
  }

  // LoRa validation
  if (config.lora.frequency < 137000000 || config.lora.frequency > 1020000000) {
    errors.push("LoRa frequency out of valid range (137–1020 MHz)");
  }
  if (config.lora.spreadingFactor < 6 || config.lora.spreadingFactor > 12) {
    errors.push("Spreading factor must be between 6 and 12");
  }
  if (config.lora.txPower < 2 || config.lora.txPower > 20) {
    errors.push("TX power must be between 2 and 20 dBm");
  }

  // RTCM validation
  if (config.rtcm.messageTypes.length === 0) {
    errors.push("At least one RTCM message type must be configured");
  }

  return errors;
}

// Save configuration
async function saveConfig(config) {
  try {
    const response = await fetch("/api/config", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(config),
    });

    const result = await response.json();

    if (!response.ok || !result.success) {
      throw new Error(result.message || "Failed to save configuration");
    }

    showStatus("Configuration saved! Device will restart in 3 seconds...");

    // Disable form
    document
      .getElementById("configForm")
      .querySelectorAll("input, select, textarea, button")
      .forEach((el) => {
        el.disabled = true;
      });
  } catch (error) {
    console.error("Error saving config:", error);
    showStatus("Failed to save configuration: " + error.message, true);
  }
}

// Form submission
document.getElementById("configForm").addEventListener("submit", async (e) => {
  e.preventDefault();

  const config = parseFormData();
  const errors = validateConfig(config);

  if (errors.length > 0) {
    showStatus("Validation errors: " + errors.join(", "), true);
    return;
  }

  if (!confirm("Save configuration and restart device?")) {
    return;
  }

  await saveConfig(config);
});

// Reload button
document.getElementById("loadBtn").addEventListener("click", () => {
  loadConfig();
});

// Restart button
document.getElementById("restartBtn").addEventListener("click", async () => {
  if (!confirm("Restart device now?")) {
    return;
  }

  try {
    await fetch("/api/restart", { method: "POST" });
    showStatus("Device restarting...");

    // Disable form
    document
      .getElementById("configForm")
      .querySelectorAll("input, select, textarea, button")
      .forEach((el) => {
        el.disabled = true;
      });
  } catch (error) {
    console.error("Error restarting device:", error);
  }
});

// WiFi scanning modal functions
function openWiFiScanModal() {
  const modal = document.getElementById("wifiScanModal");
  modal.style.display = "block";

  // Auto-start scan when modal opens
  scanNetworksInModal();
}

function closeWiFiScanModal() {
  const modal = document.getElementById("wifiScanModal");
  modal.style.display = "none";
}

function rescanNetworks() {
  scanNetworksInModal();
}

async function scanNetworksInModal() {
  const scanStatus = document.getElementById("scanStatus");
  const networkListContainer = document.getElementById("networkListContainer");
  const networkList = document.getElementById("networkList");

  // Disable rescan button during scan
  const rescanBtn = document.querySelector("#wifiScanModal .btn-primary");
  if (rescanBtn) rescanBtn.disabled = true;

  // Show scanning status
  scanStatus.style.display = "flex";
  scanStatus.innerHTML =
    '<div class="loading"></div><p>Scanning for networks...</p>';
  networkListContainer.style.display = "none";
  networkList.innerHTML = "";

  try {
    // Simple synchronous scan - just POST and wait for response
    const response = await fetch("/api/wifi/scan", { method: "POST" });

    if (!response.ok) {
      scanStatus.innerHTML = `<p class="error">WiFi scan failed (HTTP ${response.status})</p>`;
      return;
    }

    const data = await response.json();

    if (!data.success) {
      scanStatus.innerHTML = '<p class="error">WiFi scan failed</p>';
      return;
    }

    if (data.networks && data.networks.length > 0) {
      // Sort by signal strength (RSSI)
      data.networks.sort((a, b) => b.rssi - a.rssi);

      // Build network list
      networkList.innerHTML = "";
      data.networks.forEach((net) => {
        const netItem = document.createElement("div");
        netItem.className = "network-item";
        netItem.onclick = () => selectNetworkFromModal(net.ssid);

        const signalClass = getSignalClass(net.rssi);
        const lockClass = net.encryption === "open" ? "unlocked" : "locked";

        netItem.innerHTML = `
          <div class="network-info">
            <div class="network-ssid">${net.ssid}</div>
            <div class="network-bssid">${net.bssid}</div>
            <div class="network-details">Channel ${net.channel} • ${net.rssi} dBm</div>
          </div>
          <div class="network-icons">
            <span class="signal-icon ${signalClass}"></span>
            <span class="lock-icon ${lockClass}"></span>
          </div>
        `;
        networkList.appendChild(netItem);
      });

      scanStatus.style.display = "none";
      networkListContainer.style.display = "block";
    } else {
      scanStatus.innerHTML = '<p class="error">No networks found</p>';
    }
  } catch (error) {
    console.error("Scan error:", error);
    scanStatus.innerHTML = '<p class="error">Failed to scan networks</p>';
  } finally {
    // Re-enable rescan button
    if (rescanBtn) rescanBtn.disabled = false;
  }
}

function getSignalClass(rssi) {
  if (rssi >= -50) return "signal-excellent";
  if (rssi >= -60) return "signal-good";
  if (rssi >= -70) return "signal-fair";
  return "signal-weak";
}

function selectNetworkFromModal(ssid) {
  document.getElementById("wifiSSID").value = ssid;
  closeWiFiScanModal();
  showStatus(`Selected network: ${ssid}`);
}

// Close modal when clicking outside
window.onclick = function (event) {
  const wifiModal = document.getElementById("wifiScanModal");
  const mountpointModal = document.getElementById("mountpointModal");

  if (event.target === wifiModal) {
    closeWiFiScanModal();
  }
  if (event.target === mountpointModal) {
    closeMountpointModal();
  }
};

// Load configuration on page load
window.addEventListener("DOMContentLoaded", () => {
  loadConfig();
});

// NTRIP Mountpoint modal functions
function openMountpointModal() {
  const host = document.getElementById("ntripHost").value;
  const port = document.getElementById("ntripPort").value;

  // Validate inputs
  if (!host || !port) {
    showStatus("Please enter NTRIP host and port first", true);
    return;
  }

  const modal = document.getElementById("mountpointModal");
  modal.style.display = "block";

  // Auto-start fetch when modal opens
  fetchMountpointsInModal();
}

function closeMountpointModal() {
  const modal = document.getElementById("mountpointModal");
  modal.style.display = "none";
}

function refetchMountpoints() {
  fetchMountpointsInModal();
}

async function fetchMountpointsInModal() {
  const host = document.getElementById("ntripHost").value;
  const port = document.getElementById("ntripPort").value;
  const user = document.getElementById("ntripUser").value;
  const password = document.getElementById("ntripPassword").value;

  const mountpointStatus = document.getElementById("mountpointStatus");
  const mountpointListContainer = document.getElementById(
    "mountpointListContainer",
  );
  const mountpointList = document.getElementById("mountpointList");

  // Disable refresh button during fetch
  const refreshBtn = document.querySelector("#mountpointModal .btn-primary");
  if (refreshBtn) refreshBtn.disabled = true;

  // Show loading status
  mountpointStatus.style.display = "flex";
  mountpointStatus.innerHTML =
    '<div class="loading"></div><p>Fetching mountpoints...</p>';
  mountpointListContainer.style.display = "none";
  mountpointList.innerHTML = "";

  try {
    // Create form data
    const formData = new URLSearchParams();
    formData.append("host", host);
    formData.append("port", port);
    if (user) formData.append("user", user);
    if (password) formData.append("password", password);

    const response = await fetch("/api/ntrip/mountpoints", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: formData.toString(),
    });

    if (!response.ok) {
      mountpointStatus.innerHTML = `<p class="error">Fetch failed (HTTP ${response.status})</p>`;
      return;
    }

    const data = await response.json();

    if (data.error) {
      mountpointStatus.innerHTML = `<p class="error">${data.error}</p>`;
      return;
    }

    if (data.mountpoints && data.mountpoints.length > 0) {
      // Build mountpoint list
      mountpointList.innerHTML = "";
      data.mountpoints.forEach((mp) => {
        const mpItem = document.createElement("div");
        mpItem.className = "network-item";
        mpItem.onclick = () => selectMountpointFromModal(mp);

        mpItem.innerHTML = `
          <div class="network-info">
            <div class="network-ssid">${mp}</div>
          </div>
        `;
        mountpointList.appendChild(mpItem);
      });

      mountpointStatus.style.display = "none";
      mountpointListContainer.style.display = "block";
    } else {
      mountpointStatus.innerHTML = '<p class="error">No mountpoints found</p>';
    }
  } catch (error) {
    console.error("Mountpoint fetch error:", error);
    mountpointStatus.innerHTML =
      '<p class="error">Failed to fetch mountpoints</p>';
  } finally {
    // Re-enable refresh button
    const refreshBtn = document.querySelector("#mountpointModal .btn-primary");
    if (refreshBtn) refreshBtn.disabled = false;
  }
}

function selectMountpointFromModal(mountpoint) {
  document.getElementById("ntripMountpoint").value = mountpoint;
  closeMountpointModal();
  showStatus(`Selected mountpoint: ${mountpoint}`);
}
