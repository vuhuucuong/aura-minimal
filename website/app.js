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
function renderPreview() {
  const screen = document.querySelector('#screen-image');
  screen.src = `assets/previews/${view === 'settings' ? 'settings' : `${view}-${weather}`}.png`;
  screen.alt = view === 'settings' ? 'Aura Minimal settings layout preview' : `Aura Minimal ${view} layout preview with sample weather data; ${weather} icon`;
  document.querySelectorAll('.preview-tabs [data-view]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.view === view)));
  document.querySelectorAll('[data-weather]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.weather === weather)));
  document.querySelector('.touch-regions').hidden = view === 'settings';
}
document.querySelectorAll('[data-view]').forEach(button => button.addEventListener('click', () => { view = button.dataset.view; renderPreview(); }));
document.querySelectorAll('[data-weather]').forEach(button => button.addEventListener('click', () => { weather = button.dataset.weather; renderPreview(); }));
if (window.isSecureContext && 'serial' in navigator) {
  document.querySelector('#connection-note').textContent = 'USB installation is available. Select your device and confirm in the installer to continue.';
}
