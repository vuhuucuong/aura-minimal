const variant = document.querySelector('#variant');
const installer = document.querySelector('#installer');
const download = document.querySelector('#firmware-link');
variant.addEventListener('change', () => {
  const inverted = variant.value === 'inverted';
  installer.setAttribute('manifest', inverted ? 'bin/manifest-inverted.json' : 'bin/manifest.json');
  download.href = inverted ? 'bin/aura-minimal-inverted.bin' : 'bin/aura-minimal.bin';
});
let view = 'daily';
let weather = 'partly';
let outlook = 'rain';
function renderPreview() {
  const screen = document.querySelector('#screen-image');
  const suffix = outlook === 'rain' ? '' : `-${outlook}`;
  screen.src = `assets/previews/${view === 'settings' ? 'settings' : `${view}-${weather}${suffix}`}.png`;
  screen.alt = view === 'settings' ? 'Aura Minimal settings layout preview' : `Aura Minimal ${view} layout preview with sample weather data; ${weather} conditions; ${outlook} outlook`;
  document.querySelectorAll('.preview-tabs [data-view]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.view === view)));
  document.querySelectorAll('[data-weather]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.weather === weather)));
  document.querySelectorAll('[data-outlook]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.outlook === outlook)));
  document.querySelector('.touch-regions').hidden = view === 'settings';
  document.querySelector('#outlook-touch').hidden = view === 'settings';
  document.querySelector('.outlook-picker').hidden = view === 'settings';
}
document.querySelectorAll('[data-view]').forEach(button => button.addEventListener('click', () => { view = button.dataset.view; renderPreview(); }));
document.querySelectorAll('[data-weather]').forEach(button => button.addEventListener('click', () => { weather = button.dataset.weather; renderPreview(); }));
document.querySelectorAll('[data-outlook]').forEach(button => button.addEventListener('click', () => { outlook = button.dataset.outlook; renderPreview(); }));
document.querySelector('#outlook-touch').addEventListener('click', () => { view = 'hourly'; renderPreview(); });
if (window.isSecureContext && 'serial' in navigator) {
  document.querySelector('#connection-note').textContent = 'USB installation is available. Select your device and confirm in the installer to continue.';
}
