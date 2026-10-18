// Prototype page renderer: draws libmspub drawing events as one SVG per page.
// Shapes are SVG, text frames are HTML inside foreignObject so the browser wraps lines.
(function () {
  const DPI = 96, NS = 'http://www.w3.org/2000/svg', XH = 'http://www.w3.org/1999/xhtml';
  const RE = /^(-?[\d.]+(?:e[-+]?\d+)?)\s*([a-z%]*)$/i;
  function num(v) { const m = v == null ? null : String(v).trim().match(RE); return m ? parseFloat(m[1]) : NaN; }
  function len(v, d = 0) { const m = v == null ? null : String(v).trim().match(RE); if (!m) return d; const n = parseFloat(m[1]), u = (m[2] || 'in').toLowerCase();
    return u === 'in' ? n * DPI : u === 'pt' ? n * DPI / 72 : u === 'cm' ? n * DPI / 2.54 : u === 'mm' ? n * DPI / 25.4 : u === 'px' ? n : u === 'twip' ? n * DPI / 1440 : n * DPI; }
  function pct(v, d = 1) { if (v == null) return d; const s = String(v).trim(); if (s.endsWith('%')) return parseFloat(s) / 100; const n = parseFloat(s); return isNaN(n) ? d : (n > 1 ? n / 100 : n); }
  function el(tag, a, ns = NS) { const e = document.createElementNS(ns, tag); if (a) for (const k in a) if (a[k] != null) e.setAttribute(k, a[k]); return e; }
  const SERIF = /times|roman|garamond|georgia|cambria|book|palatino|schoolbook|bodoni|baskerville|perpetua|rockwell|goudy|bookman|constantia|minion/i;
  function fontStack(n) { if (!n) return 'serif'; n = n.replace(/"/g, ''); return `"${n}", ${SERIF.test(n) ? 'serif' : 'sans-serif'}`; }
  const BAD_MIME = /pict/i;
  const imgCache = new Map();
  function b64ToBuf(b64) { const bin = atob(b64), u = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) u[i] = bin.charCodeAt(i); return u.buffer; }
  function svgUrl(svg) { svg.setAttribute('preserveAspectRatio', 'none'); return 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(new XMLSerializer().serializeToString(svg)); }
  // Converts formats browsers cannot show (WMF, EMF, TIFF) into SVG or PNG data URLs; null means unsupported.
  function toDataUrl(mime, b64) {
    if (!b64) return null;
    const key = mime + b64.length + b64.slice(0, 48) + b64.slice(-48);
    if (imgCache.has(key)) return imgCache.get(key);
    let url = null;
    try {
      if (/wmf/i.test(mime)) url = window.WMFJS ? svgUrl(new WMFJS.Renderer(b64ToBuf(b64)).render({ width: '1000px', height: '1000px', xExt: 1000, yExt: 1000, mapMode: 8 })) : null;
      else if (/emf/i.test(mime)) url = window.EMFJS ? svgUrl(new EMFJS.Renderer(b64ToBuf(b64)).render({ width: '1000px', height: '1000px', wExt: 1000, hExt: 1000, xExt: 1000, yExt: 1000, mapMode: 8 })) : null;
      else if (/tif/i.test(mime)) {
        if (window.UTIF) { const buf = b64ToBuf(b64), ifds = UTIF.decode(buf); UTIF.decodeImage(buf, ifds[0]); const rgba = UTIF.toRGBA8(ifds[0]);
          const c = document.createElement('canvas'); c.width = ifds[0].width; c.height = ifds[0].height;
          c.getContext('2d').putImageData(new ImageData(new Uint8ClampedArray(rgba.buffer, rgba.byteOffset, c.width * c.height * 4), c.width, c.height), 0, 0); url = c.toDataURL('image/png'); }
      } else if (!BAD_MIME.test(mime)) url = `data:${mime};base64,${b64}`;
    } catch (err) { url = null; }
    imgCache.set(key, url); return url;
  }
  window.renderPub = function (events, host) {
    let svg = null, defs = null, style = {}, gid = 0, pages = 0, badImages = 0, flow = [], para = null, span = null, row = null, tbl = null;
    function newPage(p) {
      const w = len(p['svg:width'], 816), h = len(p['svg:height'], 1056);
      const div = document.createElement('div'); div.className = 'pubpage';
      div.style.cssText = `position:relative;width:${w}px;height:${h}px;background:#fff;overflow:hidden;margin-bottom:12px`;
      svg = el('svg', { width: w, height: h, viewBox: `0 0 ${w} ${h}` }); svg.style.cssText = 'position:absolute;left:0;top:0';
      defs = el('defs'); svg.appendChild(defs); div.appendChild(svg); host.appendChild(div); pages++;
    }
    function gradient(s) {
      const id = 'g' + gid++, g = el('linearGradient', { id, x1: 0, y1: 0, x2: 0, y2: 1, gradientTransform: `rotate(${-(num(s['draw:angle']) || 0)} .5 .5)` });
      const st = s['svg:linearGradient'];
      if (Array.isArray(st) && st.length) for (const x of st) g.appendChild(el('stop', { offset: x['svg:offset'] || '0%', 'stop-color': x['svg:stop-color'] || '#fff', 'stop-opacity': pct(x['svg:stop-opacity'], 1) }));
      else { g.appendChild(el('stop', { offset: '0%', 'stop-color': s['draw:start-color'] || s['draw:fill-color'] || '#fff' })); g.appendChild(el('stop', { offset: '100%', 'stop-color': s['draw:end-color'] || '#000' })); }
      defs.appendChild(g); return `url(#${id})`;
    }
    function bitmap(s) {
      const url = toDataUrl(s['librevenge:mime-type'] || 'image/png', s['draw:fill-image']);
      if (!url) { badImages++; return '#d9d9d9'; }
      const id = 'p' + gid++, pat = el('pattern', { id, patternUnits: 'objectBoundingBox', patternContentUnits: 'objectBoundingBox', width: 1, height: 1 });
      pat.appendChild(el('image', { width: 1, height: 1, preserveAspectRatio: 'none', href: url }));
      defs.appendChild(pat); return `url(#${id})`;
    }
    function thinShape(pts) {
      if (pts.length < 2) return false;
      let a = 0, per = 0;
      for (let i = 0; i < pts.length; i++) { const [x1, y1] = pts[i], [x2, y2] = pts[(i + 1) % pts.length]; a += x1 * y2 - x2 * y1; per += Math.hypot(x2 - x1, y2 - y1); }
      return per > 4 && Math.abs(a) / per < 0.6;
    }
    function paintMaybeHairline(n, pts) {
      const s = style;
      if ((s['draw:stroke'] || 'none') === 'none' && s['draw:fill'] === 'solid' && thinShape(pts)) {
        style = Object.assign({}, s, { 'draw:fill': 'none', 'draw:stroke': 'solid', 'svg:stroke-color': s['draw:fill-color'] || '#000', 'svg:stroke-width': '0.75pt' });
        paint(n); style = s;
      } else paint(n);
    }
    function paint(n) {
      const s = style, f = s['draw:fill'] || 'none';
      if (f === 'solid') { n.setAttribute('fill', s['draw:fill-color'] || '#000'); n.setAttribute('fill-opacity', pct(s['draw:opacity'], 1)); }
      else if (f === 'gradient') n.setAttribute('fill', gradient(s));
      else if (f === 'bitmap' && s['draw:fill-image']) n.setAttribute('fill', bitmap(s));
      else n.setAttribute('fill', 'none');
      if ((s['draw:stroke'] || 'none') !== 'none') {
        n.setAttribute('stroke', s['svg:stroke-color'] || '#000'); n.setAttribute('stroke-width', Math.max(len(s['svg:stroke-width'], 1), 0.6));
        if (s['draw:stroke'] === 'dash') n.setAttribute('stroke-dasharray', '6 4');
        if (s['svg:stroke-linecap']) n.setAttribute('stroke-linecap', s['svg:stroke-linecap']);
      }
      if (s['svg:fill-rule']) n.setAttribute('fill-rule', s['svg:fill-rule']);
      svg.appendChild(n);
    }
    const P = v => len(v).toFixed(2);
    function pathD(vec) {
      let d = '';
      for (const c of vec || []) {
        const a = String(c['librevenge:path-action'] || '').toUpperCase();
        if (a === 'M' || a === 'L') d += `${a}${P(c['svg:x'])},${P(c['svg:y'])} `;
        else if (a === 'C') d += `C${P(c['svg:x1'])},${P(c['svg:y1'])} ${P(c['svg:x2'])},${P(c['svg:y2'])} ${P(c['svg:x'])},${P(c['svg:y'])} `;
        else if (a === 'Q') d += `Q${P(c['svg:x1'])},${P(c['svg:y1'])} ${P(c['svg:x'])},${P(c['svg:y'])} `;
        else if (a === 'A') d += `A${P(c['svg:rx'])},${P(c['svg:ry'])} ${num(c['librevenge:rotate']) || 0} ${c['librevenge:large-arc'] === 'true' ? 1 : 0},${c['librevenge:sweep'] === 'true' ? 1 : 0} ${P(c['svg:x'])},${P(c['svg:y'])} `;
        else if (a === 'Z') d += 'Z ';
      }
      return d;
    }
    function frame(p, isTable) {
      const x = len(p['svg:x']), y = len(p['svg:y']), w = Math.max(len(p['svg:width']), 1), h = Math.max(len(p['svg:height']), 1);
      const g = el('g'), rot = num(p['librevenge:rotate']);
      if (rot) g.setAttribute('transform', `rotate(${-rot} ${x + w / 2} ${y + h / 2})`);
      const fo = el('foreignObject', { x, y, width: w, height: h }); fo.style.overflow = 'visible';
      const box = document.createElementNS(XH, 'div');
      if (isTable) box.style.cssText = `width:${w}px;height:${h}px`;
      else {
        const va = p['draw:textarea-vertical-align'] || 'top';
        box.style.cssText = `box-sizing:border-box;width:${w}px;height:${h}px;display:flex;flex-direction:column;justify-content:${va === 'middle' ? 'center' : va === 'bottom' ? 'flex-end' : 'flex-start'};` +
          `padding:${len(p['fo:padding-top'])}px ${len(p['fo:padding-right'])}px ${len(p['fo:padding-bottom'])}px ${len(p['fo:padding-left'])}px;white-space:pre-wrap;overflow-wrap:break-word;tab-size:48px;font-size:13.33px;line-height:normal;color:#000`;
        const cols = parseInt(p['fo:column-count'] || '1', 10);
        if (cols > 1) { box.style.display = 'block'; box.style.columnCount = cols; box.style.columnGap = len(p['fo:column-gap'], 12) + 'px'; }
      }
      fo.appendChild(box); g.appendChild(fo); svg.appendChild(g); return box;
    }
    function cur() { return flow[flow.length - 1]; }
    function openPara(p) {
      const d = document.createElementNS(XH, 'div'), s = d.style;
      s.margin = `${len(p['fo:margin-top'])}px ${len(p['fo:margin-right'])}px ${len(p['fo:margin-bottom'])}px ${len(p['fo:margin-left'])}px`;
      if (p['fo:text-indent']) s.textIndent = len(p['fo:text-indent']) + 'px';
      const ta = p['fo:text-align']; if (ta) s.textAlign = ta === 'end' ? 'right' : ta === 'start' ? 'left' : ta;
      const lh = p['fo:line-height'];
      if (lh) { if (String(lh).endsWith('%')) s.lineHeight = (pct(lh) * 1.17).toFixed(3); else s.lineHeight = len(lh) + 'px'; }
      (cur() || frame({}, false)).appendChild(d); para = d; span = null;
    }
    function openSpan(p) {
      const sp = document.createElementNS(XH, 'span'), s = sp.style;
      if (p['style:font-name']) s.fontFamily = fontStack(p['style:font-name']);
      if (p['fo:font-size']) s.fontSize = len(p['fo:font-size'], 13.33) + 'px';
      if (p['fo:font-weight']) s.fontWeight = p['fo:font-weight'];
      if (p['fo:font-style']) s.fontStyle = p['fo:font-style'];
      if (p['fo:color']) s.color = p['fo:color'];
      if (p['fo:font-variant'] === 'small-caps') s.fontVariant = 'small-caps';
      if (p['fo:text-transform']) s.textTransform = p['fo:text-transform'];
      if (p['style:text-underline-type'] && p['style:text-underline-type'] !== 'none') s.textDecoration = 'underline';
      if (!para) openPara({});
      para.appendChild(sp); span = sp;
    }
    function text(t) { if (!para) openPara({}); (span || para).appendChild(document.createTextNode(t)); }
    for (const e of events) {
      const t = e[0], p = e[1] || {};
      switch (t) {
        case 'startPage': newPage(p); break;
        case 'setStyle': style = p; break;
        case 'drawRectangle': paint(el('rect', { x: len(p['svg:x']), y: len(p['svg:y']), width: len(p['svg:width']), height: len(p['svg:height']) })); break;
        case 'drawEllipse': paint(el('ellipse', { cx: len(p['svg:cx']), cy: len(p['svg:cy']), rx: len(p['svg:rx']), ry: len(p['svg:ry']) })); break;
        case 'drawPolygon': case 'drawPolyline': {
          const pts = (p['svg:points'] || []).map(q => `${P(q['svg:x'])},${P(q['svg:y'])}`).join(' ');
          if (t === 'drawPolyline') { const s0 = style; style = Object.assign({}, s0, { 'draw:fill': 'none' }); paint(el('polyline', { points: pts })); style = s0; }
          else paintMaybeHairline(el('polygon', { points: pts }), (p['svg:points'] || []).map(q => [len(q['svg:x']), len(q['svg:y'])]));
          break; }
        case 'drawPath': case 'drawConnector': {
          const d = pathD(p['svg:d']), n = el('path', { d });
          // A lone straight segment with fill but no stroke is a hairline rule; draw it as a thin line.
          const nums = (d.match(/-?[\d.]+/g) || []).map(Number), pts = [];
          for (let i = 0; i + 1 < nums.length; i += 2) pts.push([nums[i], nums[i + 1]]);
          paintMaybeHairline(n, /[CQA]/.test(d) ? [] : pts);
          break; }
        case 'drawGraphicObject': {
          const mime = p['librevenge:mime-type'] || 'image/png';
          if (BAD_MIME.test(mime)) { badImages++; break; }
          svg.appendChild(el('image', { x: len(p['svg:x']), y: len(p['svg:y']), width: len(p['svg:width']), height: len(p['svg:height']), preserveAspectRatio: 'none', href: `data:${mime};base64,${p['office:binary-data']}` })); break; }
        case 'startTextObject': flow = [frame(p, false)]; para = span = null; break;
        case 'endTextObject': flow = []; para = span = null; break;
        case 'startTableObject': {
          const box = frame(p, true), table = document.createElementNS(XH, 'table');
          table.style.cssText = 'border-collapse:collapse;table-layout:fixed;width:100%;height:100%;font-size:13.33px;white-space:pre-wrap';
          const cg = document.createElementNS(XH, 'colgroup');
          for (const c of p['librevenge:table-columns'] || []) { const col = document.createElementNS(XH, 'col'); col.style.width = len(c['style:column-width']) + 'px'; cg.appendChild(col); }
          table.appendChild(cg); box.appendChild(table); tbl = table; flow = []; break; }
        case 'openTableRow': row = document.createElementNS(XH, 'tr'); if (p['style:row-height']) row.style.height = len(p['style:row-height']) + 'px'; tbl && tbl.appendChild(row); break;
        case 'openTableCell': {
          const td = document.createElementNS(XH, 'td');
          if (p['table:number-columns-spanned']) td.colSpan = parseInt(p['table:number-columns-spanned'], 10);
          if (p['table:number-rows-spanned']) td.rowSpan = parseInt(p['table:number-rows-spanned'], 10);
          td.style.cssText = `vertical-align:top;padding:2px 4px;${p['fo:background-color'] ? 'background:' + p['fo:background-color'] + ';' : ''}`;
          for (const side of ['top', 'right', 'bottom', 'left']) { const b = p['fo:border-' + side]; if (b) td.style['border' + side[0].toUpperCase() + side.slice(1)] = String(b).replace(/(-?[\d.]+)in/, (m, n) => (n * DPI).toFixed(2) + 'px'); }
          row && row.appendChild(td); flow = [td]; para = span = null; break; }
        case 'closeTableCell': flow = []; para = span = null; break;
        case 'endTableObject': tbl = row = null; break;
        case 'openParagraph': case 'openListElement': openPara(p); break;
        case 'closeParagraph': case 'closeListElement': para = span = null; break;
        case 'openSpan': openSpan(p); break;
        case 'closeSpan': span = null; break;
        case 'insertText': text(e[1]); break;
        case 'insertSpace': text(' '); break;
        case 'insertTab': text('\t'); break;
        case 'insertLineBreak': if (para) para.appendChild(document.createElementNS(XH, 'br')); break;
        case 'insertField': text('#'); break;
      }
    }
    return { pages, badImages };
  };
})();
