// PUB File Opener: reads a .pub file in the browser and draws its pages. Nothing is uploaded.
(function () {
  const $ = id => document.getElementById(id);
  const drop = $('drop'), input = $('file'), status = $('status'), viewer = $('viewer'), pages = $('pages');
  let corePromise = null;
  const core = () => corePromise || (corePromise = PubCore());
  const say = msg => { status.textContent = msg; };
  function fitPages() {
    const avail = pages.clientWidth;
    for (const pg of pages.querySelectorAll('.pubpage')) {
      const w = parseFloat(pg.style.width), h = parseFloat(pg.style.height), s = Math.min(1, avail / w);
      pg.style.transformOrigin = 'top left';
      pg.style.transform = s < 1 ? `scale(${s})` : '';
      pg.style.marginBottom = s < 1 ? `${16 - h * (1 - s)}px` : '16px';
    }
  }
  async function openFile(file) {
    if (!file) return;
    say(`Opening ${file.name}…`);
    try {
      const [mod, buf] = await Promise.all([core(), file.arrayBuffer()]);
      const bytes = new Uint8Array(buf), p = mod._malloc(bytes.length), st = mod._malloc(4);
      mod.HEAPU8.set(bytes, p);
      const out = mod._pub_to_json(p, bytes.length, st), code = mod.getValue(st, 'i32');
      const json = out ? mod.UTF8ToString(out) : null;
      if (out) mod._pub_free(out);
      mod._free(p); mod._free(st);
      if (code === 3 || !json) { say('This doesn’t look like a Publisher file, or its version isn’t supported yet.'); return; }
      pages.textContent = '';
      const info = window.renderPub(JSON.parse(json), pages);
      if (!info.pages) { say('We couldn’t read any pages from this file.'); return; }
      $('docname').textContent = file.name;
      viewer.hidden = false;
      say(`${info.pages} page${info.pages === 1 ? '' : 's'}${code === 4 ? '. Parts of this file couldn’t be read.' : ''}`);
      fitPages();
      viewer.scrollIntoView({ behavior: 'smooth' });
    } catch (e) { console.error(e); say('Something went wrong opening this file.'); }
  }
  input.addEventListener('change', () => openFile(input.files[0]));
  ['dragenter', 'dragover'].forEach(t => drop.addEventListener(t, e => { e.preventDefault(); drop.classList.add('over'); }));
  ['dragleave', 'drop'].forEach(t => drop.addEventListener(t, e => { e.preventDefault(); drop.classList.remove('over'); }));
  drop.addEventListener('drop', e => openFile(e.dataTransfer.files[0]));
  $('print').addEventListener('click', () => window.print());
  $('close').addEventListener('click', () => { viewer.hidden = true; pages.textContent = ''; input.value = ''; say(''); window.scrollTo({ top: 0 }); });
  window.addEventListener('resize', fitPages);
})();
