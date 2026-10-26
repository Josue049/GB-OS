(function () {
  'use strict';

  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));

  const Emu = window.WasmBoy.WasmBoy || window.WasmBoy;
  const systems = window.SYSTEMS;
  const gbc = $('#gbc');
  const lcd = $('#lcd');
  const screenEl = $('#screen');
  const msgEl = $('#screen-msg');
  const list = $('#systems');
  const toastEl = $('#toast');

  /* ───────────── Aviso ───────────── */
  let toastTimer;
  function toast(text) {
    toastEl.textContent = text;
    toastEl.classList.add('is-shown');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toastEl.classList.remove('is-shown'), 2400);
  }

  /* ───────────── Mensajes sobre la pantalla ───────────── */
  function showMsg(title, sub, wait) {
    $('#msg-title').textContent = title;
    $('#msg-sub').textContent = sub || '';
    msgEl.classList.toggle('is-wait', !!wait);
    msgEl.classList.add('is-shown');
  }
  const hideMsg = () => msgEl.classList.remove('is-shown');

  /* ───────────── Emulador (WasmBoy) ───────────── */
  let emuReady = false;   // el worker ya aceptó una ROM
  let running = false;    // hay una ROM de sistema funcionando
  let token = 0;          // descarta cargas antiguas si el usuario cambia rápido de sistema

  const emuInit = Emu.config({
    isGbcEnabled: true,
    isAudioEnabled: true,
    frameSkip: 0,
    gameboyFrameRate: 60,
    maxNumberOfAutoSaveStates: 0,
  }, lcd).then(() => Emu.disableDefaultJoypad());

  async function boot(sys) {
    const mine = ++token;
    screenEl.classList.remove('is-switching');
    void screenEl.offsetWidth;
    screenEl.classList.add('is-switching');

    if (!sys.rom) {
      running = false;
      gbc.classList.remove('is-on');
      releaseAll();
      try { await emuInit; await Emu.pause(); } catch (e) { /* aún no había nada cargado */ }
      showMsg(sys.name.toUpperCase(), 'COMING SOON', true);
      return;
    }

    gbc.classList.remove('is-on');
    showMsg('LOADING', '', true);
    try {
      const res = await fetch(sys.rom);
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const bytes = new Uint8Array(await res.arrayBuffer());
      await emuInit;
      if (mine !== token) return;
      await Emu.loadROM(bytes);
      if (mine !== token) return;
      Emu.disableDefaultJoypad();   // loadROM lo reactiva y pisaría el estado de los botones en pantalla
      await Emu.play();
      Emu.disableDefaultJoypad();
      emuReady = true;
      running = true;
      hideMsg();
      gbc.classList.add('is-on');
      applyJoypad();
    } catch (err) {
      if (mine !== token) return;
      console.error(err);
      const file = location.protocol === 'file:';
      showMsg('ERROR', file ? 'OPEN THIS PAGE WITH A LOCAL SERVER' : 'COULD NOT LOAD THE ROM');
    }
  }

  // WasmBoy se pausa solo al ocultar la pestaña; hay que reanudarlo al volver
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') { if (running) Emu.play().catch(() => {}); }
    else releaseAll();
  });

  /* ───────────── Mando ───────────── */
  const BUTTONS = ['UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'B', 'SELECT', 'START'];
  const held = Object.fromEntries(BUTTONS.map(b => [b, new Set()]));
  const lights = Object.fromEntries(BUTTONS.map(b => [b, $$(`[data-btn="${b}"], [data-light="${b}"]`)]));
  const dpadEl = $('#dpad');

  let padState = {};
  const sendPad = () => { if (emuReady && running) { try { Emu.setJoypadState(padState); } catch (e) { /* worker ocupado */ } } };
  function applyJoypad() {
    const state = {};
    for (const b of BUTTONS) {
      const down = held[b].size > 0;
      state[b] = down;
      for (const el of lights[b]) el.classList.toggle('is-down', down);
    }
    dpadEl.classList.toggle('is-down', state.UP || state.DOWN || state.LEFT || state.RIGHT);
    padState = state;
    sendPad();
  }
  // Mientras haya un botón mantenido se reenvía el estado cada frame: así no se pierde si el worker
  // estaba ocupado y el juego lo ve siempre como "mantenido"
  (function keepAlive() {
    if (BUTTONS.some(b => held[b].size)) sendPad();
    requestAnimationFrame(keepAlive);
  })();
  function press(btn, src) { if (!held[btn].has(src)) { held[btn].add(src); applyJoypad(); } }
  function release(btn, src) { if (held[btn].delete(src)) applyJoypad(); }
  function releaseSource(src) { for (const b of BUTTONS) held[b].delete(src); applyJoypad(); }
  function releaseAll() { for (const b of BUTTONS) held[b].clear(); applyJoypad(); }

  // Teclado
  const KEYS = {
    ArrowUp: 'UP', ArrowDown: 'DOWN', ArrowLeft: 'LEFT', ArrowRight: 'RIGHT',
    KeyZ: 'A', KeyX: 'B', ShiftLeft: 'SELECT', ShiftRight: 'SELECT', Enter: 'START', NumpadEnter: 'START',
  };
  const onControl = () => { const a = document.activeElement; return a && a !== document.body && /^(A|BUTTON)$/.test(a.tagName); };
  function wake() { try { Emu.resumeAudioContext(); } catch (e) { /* sin audio todavía */ } }

  addEventListener('keydown', e => {
    const btn = KEYS[e.code];
    if (!btn || e.ctrlKey || e.metaKey || e.altKey) return;
    // Enter sobre un botón enfocado lo activa como siempre; las flechas sí se reservan para el mando
    if (btn === 'START' && onControl()) return;
    e.preventDefault();
    wake();
    if (!e.repeat) press(btn, 'k:' + e.code);
  });
  addEventListener('keyup', e => {
    const btn = KEYS[e.code];
    if (btn) { e.preventDefault(); release(btn, 'k:' + e.code); }
  });
  addEventListener('blur', releaseAll);

  // Botones en pantalla (cuerpo de la consola y panel de controles)
  $$('button[data-btn]').forEach(el => {
    const btn = el.dataset.btn;
    el.addEventListener('pointerdown', e => {
      e.preventDefault(); wake();
      el.setPointerCapture(e.pointerId);
      press(btn, 'p:' + e.pointerId);
    });
    const up = e => release(btn, 'p:' + e.pointerId);
    el.addEventListener('pointerup', up);
    el.addEventListener('pointercancel', up);
    el.addEventListener('lostpointercapture', up);
    el.addEventListener('contextmenu', e => e.preventDefault());
  });

  // Cruceta: una sola zona que calcula la dirección (admite diagonales y deslizar el dedo)
  function dpadDirs(e) {
    const r = dpadEl.getBoundingClientRect();
    const dx = (e.clientX - (r.left + r.width / 2)) / (r.width / 2);
    const dy = (e.clientY - (r.top + r.height / 2)) / (r.height / 2);
    if (Math.hypot(dx, dy) < .16) return [];
    const out = [];
    if (dx < -.32) out.push('LEFT'); else if (dx > .32) out.push('RIGHT');
    if (dy < -.32) out.push('UP'); else if (dy > .32) out.push('DOWN');
    return out;
  }
  function dpadUpdate(e) {
    const src = 'p:' + e.pointerId, dirs = dpadDirs(e);
    for (const b of ['UP', 'DOWN', 'LEFT', 'RIGHT']) {
      if (dirs.includes(b)) held[b].add(src); else held[b].delete(src);
    }
    applyJoypad();
  }
  dpadEl.addEventListener('pointerdown', e => { e.preventDefault(); wake(); dpadEl.setPointerCapture(e.pointerId); dpadUpdate(e); });
  dpadEl.addEventListener('pointermove', e => { if (dpadEl.hasPointerCapture(e.pointerId)) dpadUpdate(e); });
  ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(t => dpadEl.addEventListener(t, e => releaseSource('p:' + e.pointerId)));
  dpadEl.addEventListener('contextmenu', e => e.preventDefault());

  /* ───────────── Selector de sistemas ───────────── */
  const artEl = $('#os-art');
  const downloads = $$('.js-download');
  let current = null;

  list.innerHTML = systems.map(s => `
    <li>
      <button class="sys${s.rom ? '' : ' has-tag'}" type="button" role="radio" aria-checked="false" data-id="${s.id}">
        <img class="logo-i" src="${s.logo}" alt="" draggable="false">
        <span class="sys-text"><b>${s.name}</b><small>${s.tagline}</small></span>
        <span class="tag">SOON</span>
        <svg class="chev" aria-hidden="true"><use href="#i-chevron"/></svg>
      </button>
    </li>`).join('');
  const items = $$('.sys', list);

  // Fondo: dos capas que se funden entre sí
  const layers = [$('#bg-a'), $('#bg-b')];
  let layer = 0;
  function setBackdrop(url) {
    const next = layers[1 - layer];
    const img = new Image();
    img.onload = () => {
      next.style.backgroundImage = `url("${url}")`;
      next.classList.add('is-shown');
      layers[layer].classList.remove('is-shown');
      layer = 1 - layer;
    };
    img.src = url;
  }
  systems.forEach(s => { new Image().src = s.bg; new Image().src = s.title; });

  function select(id, pushHash) {
    const sys = systems.find(s => s.id === id) || systems[0];
    if (current === sys) return;
    current = sys;

    items.forEach(b => {
      const on = b.dataset.id === sys.id;
      b.classList.toggle('is-active', on);
      b.setAttribute('aria-checked', on);
      b.tabIndex = on ? 0 : -1;
    });
    document.body.dataset.os = sys.id;
    setBackdrop(sys.bg);
    artEl.src = sys.title;
    artEl.alt = `OS/GB presents ${sys.name}. ${sys.desc}`;
    document.title = `OS/GB — ${sys.name}`;
    downloads.forEach(a => {
      a.classList.toggle('is-off', !sys.rom);
      a.setAttribute('aria-disabled', !sys.rom);
      if (sys.rom) { a.href = sys.rom; a.download = sys.file; }
      else { a.removeAttribute('download'); a.href = '#' + sys.id; }
    });
    if (pushHash) history.replaceState(null, '', '#' + sys.id);
    boot(sys);
  }

  list.addEventListener('click', e => {
    const b = e.target.closest('.sys');
    if (!b) return;
    select(b.dataset.id, true);
    if (e.detail > 0) b.blur(); // con ratón, el foco se libera para que Enter siga siendo START
  });
  list.addEventListener('keydown', e => {
    const i = items.indexOf(document.activeElement);
    if (i < 0 || !/^Arrow(Up|Down)$/.test(e.key)) return;
    // las flechas pertenecen al mando, salvo que el usuario navegue la lista con el foco puesto en ella
    e.stopPropagation();
    e.preventDefault();
    const n = items[(i + (e.key === 'ArrowDown' ? 1 : items.length - 1)) % items.length];
    n.focus();
    select(n.dataset.id, true);
  });

  downloads.forEach(a => a.addEventListener('click', e => {
    if (!current.rom) { e.preventDefault(); toast(`${current.name} is coming soon`); }
  }));

  // Pantalla completa del escenario con la consola
  const stage = $('#stage');
  $('#fs').addEventListener('click', e => {
    if (document.fullscreenElement) document.exitFullscreen();
    else if (stage.requestFullscreen) stage.requestFullscreen().catch(() => toast('Fullscreen is not available'));
    else toast('Fullscreen is not available');
    e.currentTarget.blur();
  });

  addEventListener('hashchange', () => select(location.hash.slice(1)));
  select(location.hash.slice(1) || systems[0].id);
})();
