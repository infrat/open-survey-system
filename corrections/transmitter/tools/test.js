#!/usr/bin/env node

// Test script dla NTRIP Helper
const NTRIPHelper = require("./ntrip_helper");

async function testConnection() {
  console.log("=== NTRIP Helper Test ===\n");

  // Konfiguracja testowa (RTK2GO)
  const config = {
    host: "91.198.76.2",
    port: 8086,
    mountpoint: "RTK4G_MULTI_RTCM32",
    username: "user",
    password: "password",
    logFile: "test_log.txt",
  };

  console.log("Testing connection to RTK2GO...");
  console.log(`Config: ${JSON.stringify(config, null, 2)}\n`);

  const helper = new NTRIPHelper(config);

  // Test na 30 sekund
  const testDuration = 240 * 1000;

  try {
    await helper.connect();
    console.log("✓ Connection successful!");

    helper.startGGATimer(10); // GGA co 10 sekund dla testu

    // Zatrzymaj po 30 sekundach
    setTimeout(() => {
      console.log("\n=== Test completed ===");
      helper.disconnect();
      process.exit(0);
    }, testDuration);
  } catch (error) {
    console.error("✗ Test failed:", error.message);
    process.exit(1);
  }
}

// Obsługa Ctrl+C
process.on("SIGINT", () => {
  console.log("\nTest interrupted by user");
  process.exit(0);
});

if (require.main === module) {
  testConnection();
}
