#pragma once

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Monitoring Ruang Belajar</title>
<style>
  * { box-sizing: border-box; }
  body {
    margin: 0;
    font-family: system-ui, Arial, sans-serif;
    background: #f1f3f5;
    color: #1d2327;
  }
  header {
    background: #1f4e5f;
    color: #fff;
    padding: 16px 20px;
  }
  header h1 { margin: 0; font-size: 1.25rem; }
  header p { margin: 4px 0 0; font-size: .85rem; opacity: .9; }
  main {
    max-width: 900px;
    margin: 0 auto;
    padding: 16px;
  }
  #status {
    padding: 10px 14px;
    border-radius: 6px;
    margin-bottom: 16px;
    font-size: .9rem;
    background: #fff3cd;
    color: #664d03;
  }
  #status.ok { background: #d1e7dd; color: #0a3622; }
  #status.err { background: #f8d7da; color: #58151c; }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 12px;
  }
  .card {
    background: #fff;
    border-radius: 8px;
    padding: 16px;
    border: 1px solid #d9dde1;
  }
  .card h2 {
    margin: 0 0 8px;
    font-size: .85rem;
    font-weight: 600;
    color: #495057;
  }
  .nilai { font-size: 2rem; font-weight: 700; }
  .nilai small { font-size: 1rem; font-weight: 400; color: #495057; }
  .catatan { margin-top: 6px; font-size: .8rem; color: #495057; }
  .info {
    margin-top: 16px;
    background: #fff;
    border: 1px solid #d9dde1;
    border-radius: 8px;
    padding: 12px 16px;
    font-size: .9rem;
  }
  .info dl { margin: 0; display: grid; grid-template-columns: 140px 1fr; gap: 6px 12px; }
  .info dt { color: #495057; }
  .info dd { margin: 0; }
  .bad { color: #b02a37; }
</style>
</head>
<body>
<header>
  <h1>Monitoring Ruang Belajar</h1>
  <p>ESP32 + DHT22 + LDR + PIR</p>
</header>
<main>
  <div id="status" role="status">Menghubungi ESP32...</div>

  <section class="grid" aria-label="Data sensor">
    <article class="card">
      <h2>Suhu</h2>
      <div class="nilai" id="suhu">-<small> &deg;C</small></div>
    </article>
    <article class="card">
      <h2>Kelembapan</h2>
      <div class="nilai" id="lembap">-<small> %</small></div>
    </article>
    <article class="card">
      <h2>Cahaya</h2>
      <div class="nilai" id="cahaya">-</div>
      <div class="catatan" id="ldrRaw"></div>
    </article>
    <article class="card">
      <h2>Gerakan</h2>
      <div class="nilai" id="gerak">-</div>
      <div class="catatan" id="gerakTerakhir"></div>
    </article>
  </section>

  <section class="info" aria-label="Info perangkat">
    <dl>
      <dt>Uptime</dt><dd id="uptime">-</dd>
      <dt>WiFi</dt><dd id="ssid">-</dd>
      <dt>IP ESP32</dt><dd id="ip">-</dd>
      <dt>Sinyal</dt><dd id="rssi">-</dd>
      <dt>Update terakhir</dt><dd id="waktu">-</dd>
    </dl>
  </section>
</main>

<script>
const INTERVAL_MS = 2500;
const $ = id => document.getElementById(id);

function setStatus(teks, kelas) {
  const el = $('status');
  el.textContent = teks;
  el.className = kelas || '';
}

function formatDurasi(detik) {
  const j = Math.floor(detik / 3600);
  const m = Math.floor((detik % 3600) / 60);
  const d = detik % 60;
  return j + ' jam ' + m + ' menit ' + d + ' detik';
}

function tampil(d) {
  if (d.dht_valid) {
    $('suhu').innerHTML = d.suhu.toFixed(1) + '<small> &deg;C</small>';
    $('lembap').innerHTML = d.kelembapan.toFixed(1) + '<small> %</small>';
  } else {
    $('suhu').innerHTML = '<span class="bad">Error DHT22</span>';
    $('lembap').innerHTML = '<span class="bad">Error DHT22</span>';
  }

  $('cahaya').textContent = d.gelap ? 'Gelap' : 'Terang';
  $('ldrRaw').textContent = 'Pin LDR DO: ' + d.ldr;

  $('gerak').textContent = d.gerakan ? 'Terdeteksi' : 'Tidak ada';
  $('gerakTerakhir').textContent = d.detik_sejak_gerakan < 0
    ? 'Belum ada gerakan sejak menyala'
    : 'Terakhir ' + d.detik_sejak_gerakan + ' detik lalu';

  $('uptime').textContent = formatDurasi(d.uptime);
  $('ssid').textContent = d.wifi_ssid;
  $('ip').textContent = d.ip;
  $('rssi').textContent = d.rssi + ' dBm';
  $('waktu').textContent = new Date().toLocaleTimeString('id-ID');
}

async function ambilData() {
  try {
    const res = await fetch('/data', { cache: 'no-store' });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const d = await res.json();
    tampil(d);
    setStatus('Terhubung ke ESP32, data diperbarui tiap ' + (INTERVAL_MS / 1000) + ' detik.', 'ok');
  } catch (e) {
    setStatus('Gagal mengambil data dari ESP32 (' + e.message + '). Cek WiFi dan alamat IP, mencoba lagi otomatis.', 'err');
  }
}

ambilData();
setInterval(ambilData, INTERVAL_MS);
</script>
</body>
</html>
)rawliteral";
