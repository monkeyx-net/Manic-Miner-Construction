import { COLOURS, COLOUR_NAMES, TILE_TYPES, TYPE_BADGE } from "./palette.js";
import { MINER_SPRITE, drawMiner, drawNpc, drawPortal } from "./sprites.js";
import {
  renderLevel,
  renderPreview,
  renderSlot,
  renderSpriteDesigner,
  displayColour,
  canvasToTile,
  tileSize,
  titleImg,
  GRID_W,
  GRID_H,
} from "./renderer.js";

// ── State ─────────────────────────────────────────────────────────────────────

const state = {
  levels: [],
  sprites: [],
  sprite_names: [],
  npcs: [],
  miner: [],
  portal: [],
  miner_sprite: null,
  currentLevel: 0,
  currentTool: "paint",
  currentGfxSlot: 1,
  designerSlot: 0,
  zoom: 3,
  showGrid: true,
  showSprites: true,
  showGapWarnings: true,
  isPainting: false,
  selectedNpc: -1,
  tileMasks: [],
};

// ── DOM refs ──────────────────────────────────────────────────────────────────

const $ = (id) => document.getElementById(id);
let levelCanvas, tilePreview, gfxSlotsEl, levelListEl;
let spriteDesignerCanvas;
let spriteDesignerType = 0;
let spriteDesignerFrame = 0;
let spriteDesignerInk = 7;   // palette index, 0 = transparent (erases)
let spriteDesignerClipboard = null;

// ── Entry ─────────────────────────────────────────────────────────────────────

document.addEventListener("DOMContentLoaded", () => {
  levelCanvas = $("level-canvas");
  tilePreview = $("tile-preview");
  gfxSlotsEl = $("gfx-slots");
  levelListEl = $("level-list");
  spriteDesignerCanvas = $("sprite-designer-canvas");

  titleImg.onload = () => { if (state.currentLevel === 19) refresh(); };

  populateColourSelects();
  populateTileTypeSelect();
  populateNpcGfxSelect();
  setupToolbarButtons();
  setupLevelListButtons();
  setupPropertiesEvents();
  setupCanvasEvents();
  setupSpriteDesignerEvents();
  setupImportExport();
  setupKeyboard();

  loadDefaultState();
});

// ── Colour select helpers ─────────────────────────────────────────────────────

function makeColourOptions(sel) {
  sel.innerHTML = "";
  COLOURS.forEach((hex, i) => {
    const opt = document.createElement("option");
    opt.value = i;
    opt.textContent = `${i}: ${COLOUR_NAMES[i]}`;
    opt.style.background = hex;
    opt.style.color = i < 8 && i !== 7 ? "#fff" : i === 0 ? "#888" : "#000";
    sel.appendChild(opt);
  });
}

function populateColourSelects() {
  makeColourOptions($("tile-paper"));
  makeColourOptions($("tile-ink"));
  makeColourOptions($("npc-ink"));
  makeColourOptions($("portal-c0"));
  makeColourOptions($("portal-c1"));
  makeColourOptions($("start-ink"));
  makeColourOptions($("sprite-ink-sel"));
  $("sprite-ink-sel").value = 7; // default white

  // Border colour select
  const bs = $("level-border");
  COLOURS.forEach((hex, i) => {
    const opt = document.createElement("option");
    opt.value = i;
    opt.textContent = `${i}: ${COLOUR_NAMES[i]}`;
    bs.appendChild(opt);
  });
}

function populateTileTypeSelect() {
  const sel = $("tile-type");
  sel.innerHTML = "";
  TILE_TYPES.forEach((t) => {
    const opt = document.createElement("option");
    opt.value = t.value;
    opt.textContent = t.name;
    sel.appendChild(opt);
  });
}

function populateNpcGfxSelect() {
  const sel = $("npc-gfx");
  sel.innerHTML = "";
  for (let i = 0; i < 29; i++) {
    const opt = document.createElement("option");
    opt.value = i;
    opt.textContent = state.sprite_names[i] ? `${i}: ${state.sprite_names[i]}` : `Sprite ${i}`;
    sel.appendChild(opt);
  }
}

// ── Toolbar ───────────────────────────────────────────────────────────────────

function setupToolbarButtons() {
  document.querySelectorAll(".tool-btn").forEach((btn) => {
    btn.addEventListener("click", () => setTool(btn.dataset.tool));
  });
  $("zoom-in").addEventListener("click", () => setZoom(state.zoom + 1));
  $("zoom-out").addEventListener("click", () => setZoom(state.zoom - 1));
  $("show-grid").addEventListener("change", (e) => {
    state.showGrid = e.target.checked;
    refresh();
  });
  $("show-sprites").addEventListener("change", (e) => {
    state.showSprites = e.target.checked;
    refresh();
  });
  $("show-gap-warnings").addEventListener("change", (e) => {
    state.showGapWarnings = e.target.checked;
    refresh();
  });
}

function setTool(tool) {
  state.currentTool = tool;
  state.selectedNpc = -1;
  document
    .querySelectorAll(".tool-btn")
    .forEach((b) => b.classList.toggle("active", b.dataset.tool === tool));
  updatePropsPanel();
  refresh();
}

function setZoom(z) {
  state.zoom = Math.max(1, Math.min(6, z));
  $("zoom-label").textContent = state.zoom + "×";
  refresh();
}

// ── Level list ────────────────────────────────────────────────────────────────

function setupLevelListButtons() {
  $("btn-add-level").addEventListener("click", addLevel);
  $("btn-dup-level").addEventListener("click", dupLevel);
  $("btn-del-level").addEventListener("click", delLevel);
  $("btn-level-up").addEventListener("click", () => moveLevel(-1));
  $("btn-level-down").addEventListener("click", () => moveLevel(+1));
}

function renderLevelList() {
  levelListEl.innerHTML = "";
  state.levels.forEach((lv, i) => {
    const el = document.createElement("div");
    el.className = "level-item" + (i === state.currentLevel ? " active" : "");
    el.innerHTML = `<span class="level-num">${i + 1}</span><span class="level-name-text">${lv.name || "Untitled"}</span>`;
    el.addEventListener("click", () => selectLevel(i));
    levelListEl.appendChild(el);
  });
}

function selectLevel(i) {
  state.currentLevel = i;
  state.selectedNpc = -1;
  loadLevelMeta();
  renderLevelList();
  renderGfxSlots();
  updateDesigner();
  refresh();
}

function addLevel() {
  const lv = makeDefaultLevel();
  state.levels.push(lv);
  state.npcs.push([null, null, null, null, null, null, null, null]);
  state.miner.push({ x: 2, y: 13, frame: 0, dir: 0, ink: 7 });
  state.portal.push({ x: 29, y: 13, gfx: Array(16).fill(0), colour: [7, 5] });
  selectLevel(state.levels.length - 1);
}

// Deep-copies the current level and all associated NPC/miner/portal data into a new slot immediately after.
function dupLevel() {
  if (!state.levels.length) return;
  const src = state.currentLevel;
  const lv = JSON.parse(JSON.stringify(state.levels[src]));
  lv.name += " (copy)";
  state.levels.splice(src + 1, 0, lv);
  state.npcs.splice(src + 1, 0, JSON.parse(JSON.stringify(state.npcs[src] || [])));
  state.miner.splice(src + 1, 0, JSON.parse(JSON.stringify(state.miner[src] || {})));
  state.portal.splice(src + 1, 0, JSON.parse(JSON.stringify(state.portal[src] || {})));
  selectLevel(src + 1);
}

function delLevel() {
  if (state.levels.length <= 1) return;
  if (!confirm(`Delete level "${state.levels[state.currentLevel].name}"?`))
    return;
  const i = state.currentLevel;
  state.levels.splice(i, 1);
  state.npcs.splice(i, 1);
  state.miner.splice(i, 1);
  state.portal.splice(i, 1);
  state.currentLevel = Math.min(i, state.levels.length - 1);
  selectLevel(state.currentLevel);
}

function moveLevel(dir) {
  const i = state.currentLevel;
  const j = i + dir;
  if (j < 0 || j >= state.levels.length) return;
  [state.levels[i], state.levels[j]] = [state.levels[j], state.levels[i]];
  [state.npcs[i], state.npcs[j]] = [state.npcs[j], state.npcs[i]];
  [state.miner[i], state.miner[j]] = [state.miner[j], state.miner[i]];
  [state.portal[i], state.portal[j]] = [state.portal[j], state.portal[i]];
  selectLevel(j);
}

// ── Level meta properties ─────────────────────────────────────────────────────

function loadLevelMeta() {
  const lv = currentLevel();
  if (!lv) return;
  $("level-name").value = lv.name || "";
  $("level-air").value = lv.air || 8192;
  $("level-border").value = lv.border || 0;
}

function setupPropertiesEvents() {
  $("level-name").addEventListener("input", (e) => {
    if (currentLevel()) {
      currentLevel().name = e.target.value;
      renderLevelList();
    }
  });
  $("level-air").addEventListener("change", (e) => {
    if (currentLevel()) currentLevel().air = +e.target.value;
  });
  $("level-border").addEventListener("change", (e) => {
    if (currentLevel()) currentLevel().border = +e.target.value;
  });

  // Tile designer type/colour
  $("tile-type").addEventListener("change", updateGfxInfo);
  $("tile-paper").addEventListener("change", updateGfxInfo);
  $("tile-ink").addEventListener("change", updateGfxInfo);

  // Npc props
  $("npc-gfx").addEventListener("change", saveNpcProps);
  $("npc-ink").addEventListener("change", saveNpcProps);
  $("npc-nframes").addEventListener("change", saveNpcProps);
  $("npc-move").addEventListener("change", saveNpcProps);
  $("npc-speed").addEventListener("change", saveNpcProps);
  $("npc-min").addEventListener("change", saveNpcProps);
  $("npc-max").addEventListener("change", saveNpcProps);
  $("btn-del-npc").addEventListener("click", deleteSelectedNpc);

  // Portal props
  $("portal-c0").addEventListener("change", savePortalProps);
  $("portal-c1").addEventListener("change", savePortalProps);

  // Start props
  $("start-ink").addEventListener("change", saveStartProps);
  $("start-dir").addEventListener("change", saveStartProps);
}

function updateGfxInfo() {
  const lv = currentLevel();
  if (!lv) return;
  const slot = state.designerSlot;
  const type = parseInt($("tile-type").value);
  let paper = parseInt($("tile-paper").value);
  let ink = parseInt($("tile-ink").value);
  if (!lv.info[slot]) lv.info[slot] = { colour: 0x07, type: 3 };
  // Auto-default item tiles to red ink when switching type
  if (type === 0 && ink === 0) { ink = 2; $("tile-ink").value = 2; }
  lv.info[slot].colour = (paper << 4) | ink;
  lv.info[slot].type = type;
  updateDesigner();
  renderGfxSlots();
  refresh();
}

// ── GFX Slot palette ──────────────────────────────────────────────────────────

function renderGfxSlots() {
  const lv = currentLevel();
  gfxSlotsEl.innerHTML = "";
  for (let i = 0; i < 10; i++) {
    const slot = document.createElement("div");
    slot.className =
      "gfx-slot" +
      (i === state.currentGfxSlot ? " paint-active" : "") +
      (i === state.designerSlot ? " design-active" : "");
    slot.title = `Slot ${i}: ${lv ? tileTypeName(lv, i) : ""}`;

    const cnv = document.createElement("canvas");
    if (lv) {
      renderSlot(cnv, slotMask(i), displayColour(lv.info[i]));
    } else {
      cnv.width = 32;
      cnv.height = 32;
    }
    slot.appendChild(cnv);

    // Paint select on left click, designer select on right click
    slot.addEventListener("click", () => {
      state.currentGfxSlot = i;
      renderGfxSlots();
    });
    slot.addEventListener("contextmenu", (e) => {
      e.preventDefault();
      state.designerSlot = i;
      $("designer-slot-num").textContent = i;
      updateDesigner();
      renderGfxSlots();
    });

    // Type badge
    if (lv && lv.info[i]) {
      const badge = TYPE_BADGE[lv.info[i].type];
      if (badge && badge.label) {
        const span = document.createElement("span");
        span.className = "gfx-slot-badge";
        span.textContent = badge.label;
        span.style.background = badge.bg;
        slot.appendChild(span);
      }
    }

    const num = document.createElement("span");
    num.className = "gfx-slot-num";
    num.textContent = i;
    slot.appendChild(num);

    gfxSlotsEl.appendChild(slot);
  }
}

function tileTypeName(lv, slotIdx) {
  if (!lv.info[slotIdx]) return "Space";
  const t = TILE_TYPES.find((x) => x.value === lv.info[slotIdx].type);
  return t ? t.name : "?";
}

function slotMask(slot) {
  const masks = state.tileMasks[state.currentLevel];
  return masks && masks[slot];
}

// ── Tile properties ───────────────────────────────────────────────────────────

function updateDesigner() {
  const lv = currentLevel();
  if (!lv) return;
  const slot = state.designerSlot;
  const info = lv.info[slot] || { colour: 0x07, type: 3 };
  const paper = (info.colour >> 4) & 0xf;
  const ink = info.colour & 0xf;

  $("tile-type").value = info.type;
  $("tile-paper").value = paper;
  $("tile-ink").value = ink;
  $("designer-slot-num").textContent = slot;

  renderPreview(tilePreview, slotMask(slot), displayColour(info));
}

// ── Main canvas events ────────────────────────────────────────────────────────

function setupCanvasEvents() {
  levelCanvas.addEventListener("mousedown", (e) => {
    e.preventDefault();
    state.isPainting = true;
    handleCanvasClick(e);
  });
  levelCanvas.addEventListener("mousemove", (e) => {
    if (!state.isPainting) return;
    if (state.currentTool === "paint" || state.currentTool === "erase") {
      handleCanvasClick(e);
    }
  });
  window.addEventListener("mouseup", () => {
    state.isPainting = false;
  });
  levelCanvas.addEventListener("contextmenu", (e) => {
    e.preventDefault();
    // Right click = erase
    const pos = canvasToTile(levelCanvas, e.clientX, e.clientY, state.zoom);
    paintTile(pos.index, 0);
    refresh();
  });
}

function handleCanvasClick(e) {
  const lv = currentLevel();
  if (!lv) return;
  const pos = canvasToTile(levelCanvas, e.clientX, e.clientY, state.zoom);

  switch (state.currentTool) {
    case "paint":
      paintTile(pos.index, state.currentGfxSlot);
      break;
    case "erase":
      paintTile(pos.index, 0);
      break;
    case "fill":
      floodFill(pos.index, state.currentGfxSlot);
      break;
    case "npc":
      handleNpcClick(pos);
      break;
    case "portal":
      placePortal(pos);
      break;
    case "start":
      placeMinerStart(pos);
      break;
  }
  refresh();
}

function paintTile(index, value) {
  const lv = currentLevel();
  if (!lv) return;
  lv.data[index] = value;
}

// 4-connected flood fill across the 32×16 tile grid, replacing all contiguous tiles matching
// the target value with the new value using an iterative stack.
function floodFill(startIndex, value) {
  const lv = currentLevel();
  if (!lv) return;
  const target = lv.data[startIndex];
  if (target === value) return;
  const stack = [startIndex];
  const visited = new Uint8Array(512);
  while (stack.length) {
    const i = stack.pop();
    if (i < 0 || i >= 512 || visited[i] || lv.data[i] !== target) continue;
    visited[i] = 1;
    lv.data[i] = value;
    const row = Math.floor(i / GRID_W);
    const col = i % GRID_W;
    if (col > 0) stack.push(i - 1);
    if (col < GRID_W - 1) stack.push(i + 1);
    if (row > 0) stack.push(i - GRID_W);
    if (row < GRID_H - 1) stack.push(i + GRID_W);
  }
}

// ── Npc tool ────────────────────────────────────────────────────────────────

function handleNpcClick(pos) {
  const lvNpcs = state.npcs[state.currentLevel] || [];
  // Check if clicking within an existing npc's 2×2-tile bounding box (16px = 2 tiles)
  const hit = lvNpcs.findIndex(
    (r) => r &&
      pos.col >= r.x && pos.col < r.x + 2 &&
      pos.row >= r.y && pos.row < r.y + 2,
  );
  if (hit >= 0) {
    state.selectedNpc = hit;
    updatePropsPanel();
    return;
  }
  // Place a new npc if below 8
  const emptySlot = lvNpcs.findIndex((r) => !r);
  if (emptySlot < 0 && lvNpcs.filter(Boolean).length >= 8) return;

  if (!state.npcs[state.currentLevel]) state.npcs[state.currentLevel] = [];
  const slot =
    emptySlot >= 0 ? emptySlot : state.npcs[state.currentLevel].length;
  while (state.npcs[state.currentLevel].length <= slot)
    state.npcs[state.currentLevel].push(null);

  state.npcs[state.currentLevel][slot] = {
    x: pos.col,
    y: pos.row,
    min: pos.col * 8,
    max: (pos.col + 4) * 8,
    move: 2,
    speed: 0,
    gfx: 0,
    ink: 2,
    nframes: 7,
    frame: 0,
    solar_powered_generator: 0,
  };
  state.selectedNpc = slot;
  updatePropsPanel();
}

function saveNpcProps() {
  const npcs = state.npcs[state.currentLevel];
  if (!npcs || state.selectedNpc < 0) return;
  const r = npcs[state.selectedNpc];
  if (!r) return;
  r.gfx = parseInt($("npc-gfx").value);
  r.ink = parseInt($("npc-ink").value);
  r.nframes = parseInt($("npc-nframes").value);
  r.move = parseInt($("npc-move").value);
  r.speed = parseInt($("npc-speed").value);
  r.min = parseInt($("npc-min").value);
  r.max = parseInt($("npc-max").value);
  updateNpcPreview(r);
  refresh();
}

function deleteSelectedNpc() {
  const npcs = state.npcs[state.currentLevel];
  if (!npcs || state.selectedNpc < 0) return;
  npcs[state.selectedNpc] = null;
  state.selectedNpc = -1;
  updatePropsPanel();
  refresh();
}

function updateNpcPreview(npc) {
  const container = $("npc-sprite-preview");
  container.innerHTML = "";
  const cnv = document.createElement("canvas");
  const ps = 2;
  cnv.width = 16 * ps;
  cnv.height = 16 * ps;
  const ctx = cnv.getContext("2d");
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, cnv.width, cnv.height);
  const sp = state.sprites[npc.gfx];
  const ink = COLOURS[npc.ink & 0xf] || "#fff";
  drawNpc(ctx, sp, 0, ink, 0, 0, ps);
  container.appendChild(cnv);
}

// ── Portal & miner start ──────────────────────────────────────────────────────

function placePortal(pos) {
  if (!state.portal[state.currentLevel]) {
    state.portal[state.currentLevel] = {
      x: pos.col,
      y: pos.row,
      gfx: Array(16).fill(0),
      colour: [7, 5],
    };
  } else {
    state.portal[state.currentLevel].x = pos.col;
    state.portal[state.currentLevel].y = pos.row;
  }
}

function placeMinerStart(pos) {
  if (!state.miner[state.currentLevel]) {
    state.miner[state.currentLevel] = {
      x: pos.col,
      y: pos.row,
      frame: 0,
      dir: 0,
      ink: 7,
    };
  } else {
    state.miner[state.currentLevel].x = pos.col;
    state.miner[state.currentLevel].y = pos.row;
  }
  updatePropsPanel();
}

function savePortalProps() {
  const p = state.portal[state.currentLevel];
  if (!p) return;
  p.colour[0] = parseInt($("portal-c0").value);
  p.colour[1] = parseInt($("portal-c1").value);
  updatePortalPreview(p);
  refresh();
}

function saveStartProps() {
  const m = state.miner[state.currentLevel];
  if (!m) return;
  m.ink = parseInt($("start-ink").value);
  m.dir = parseInt($("start-dir").value);
  updateMinerPreview(m);
  refresh();
}

function updatePortalPreview(p) {
  const container = $("portal-sprite-preview");
  container.innerHTML = "";
  const cnv = document.createElement("canvas");
  const ps = 2;
  cnv.width = 16 * ps;
  cnv.height = 16 * ps;
  const ctx = cnv.getContext("2d");
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, cnv.width, cnv.height);
  drawPortal(ctx, p.gfx, p.colour[0], p.colour[1], 0, 0, ps);
  container.appendChild(cnv);
}

function updateMinerPreview(m) {
  const container = $("miner-sprite-preview");
  container.innerHTML = "";
  const cnv = document.createElement("canvas");
  const ps = 2;
  cnv.width = 16 * ps;
  cnv.height = 16 * ps;
  const ctx = cnv.getContext("2d");
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, cnv.width, cnv.height);
  const ink = COLOURS[m.ink & 0xf] || "#fff";
  drawMiner(ctx, m.frame || 0, ink, 0, 0, ps, m.dir);
  container.appendChild(cnv);
}

// ── Properties panel ──────────────────────────────────────────────────────────

function updatePropsPanel() {
  const tool = state.currentTool;
  $("npc-props").style.display = "none";
  $("portal-props").style.display = "none";
  $("start-props").style.display = "none";
  $("tile-info").style.display = "none";

  if (tool === "npc" && state.selectedNpc >= 0) {
    $("npc-props").style.display = "block";
    const npcs = state.npcs[state.currentLevel] || [];
    const r = npcs[state.selectedNpc];
    if (r) {
      $("npc-gfx").value = r.gfx;
      $("npc-ink").value = r.ink;
      $("npc-nframes").value = r.nframes;
      $("npc-move").value = r.move;
      $("npc-speed").value = r.speed;
      $("npc-min").value = r.min;
      $("npc-max").value = r.max;
      updateNpcPreview(r);
    }
  } else if (tool === "portal") {
    $("portal-props").style.display = "block";
    const p = state.portal[state.currentLevel];
    if (p) {
      $("portal-c0").value = p.colour[0];
      $("portal-c1").value = p.colour[1];
      updatePortalPreview(p);
    }
  } else if (tool === "start") {
    $("start-props").style.display = "block";
    const m = state.miner[state.currentLevel];
    if (m) {
      $("start-ink").value = m.ink;
      $("start-dir").value = m.dir;
      updateMinerPreview(m);
    }
  } else {
    $("tile-info").style.display = "block";
  }
}

// ── Import / Export ───────────────────────────────────────────────────────────

function setupImportExport() {
  const fileInput = $("file-input");
  $("btn-import").addEventListener("click", () => fileInput.click());
  fileInput.addEventListener("change", (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (ev) => {
      try {
        const json = JSON.parse(ev.target.result);
        loadJSON(json);
      } catch (err) {
        alert("Invalid JSON: " + err.message);
      }
    };
    reader.readAsText(file);
    fileInput.value = "";
  });

  $("btn-export").addEventListener("click", exportJSON);
  $("btn-export-sprites").addEventListener("click", exportSprites);
}

function loadJSON(json) {
  state.levels = (json.levels || []).map(normaliseLevel);
  state.sprites = json.sprites || [];
  state.sprite_names = json.sprite_names || [];
  populateNpcGfxSelect();
  state.npcs = json.npcs || [];
  state.miner = json.miner || [];
  state.portal = json.portal || [];
  state.miner_sprite = json.miner_sprite || null;

  const n = state.levels.length;
  while (state.npcs.length < n)
    state.npcs.push([null, null, null, null, null, null, null, null]);
  while (state.miner.length < n)
    state.miner.push({ x: 2, y: 13, frame: 0, dir: 0, ink: 7 });
  while (state.portal.length < n)
    state.portal.push({ x: 29, y: 13, gfx: Array(16).fill(0), colour: [7, 5] });

  state.currentLevel = 0;
  state.selectedNpc = -1;
  loadLevelMeta();
  renderLevelList();
  renderGfxSlots();
  updateDesigner();
  refresh();
}

// Ensures a level object has full-length data/gfx/info arrays.
function normaliseLevel(lv) {
  if (!lv.data || lv.data.length < 512) {
    lv.data = Array(512).fill(0);
  }
  if (!lv.info)
    lv.info = Array(10).fill(null).map(() => ({ colour: 0x07, type: 3 }));
  delete lv.gfx;
  while (lv.info.length < 10) lv.info.push({ colour: 0x07, type: 3 });
  return lv;
}

// Serialises the full game state to levels.json; post-processes the output to collapse
// simple numeric/string arrays onto a single line for readability.
function exportJSON() {
  const json = {
    levels: state.levels,
    sprites: state.sprites.map((type) => type.map((f) => (f.length === 256 ? packU16Frame(f) : f))),
    ...(state.sprite_names.length ? { sprite_names: state.sprite_names } : {}),
    npcs: state.npcs,
    miner: state.miner,
    portal: state.portal,
    ...(state.miner_sprite ? { miner_sprite: state.miner_sprite } : {}),
  };
  let json_text = JSON.stringify(json, null, 2);
  json_text = json_text.replace(/\[\s+([^\[\]\{]*?)\s+\]/g, (match, p1) => {
    return "[" + p1.replace(/\s+/g, " ").trim() + "]";
  });

  const blob = new Blob([json_text], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = "levels.json";
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// ── Export sprite sheets ──────────────────────────────────────────────────────

/*
 * Render a 1-bit sprite word into a canvas context at (ox, oy).
 * Each bit maps to a 2×2 block so the output is 32×32 per sprite frame.
 *   lsb  = true  → bit 0 is leftmost (miner convention)
 *   lsb  = false → bit 15 is leftmost (npc/portal convention)
 */
function renderSpriteRow(ctx, word, ox, oy, lsb) {
  for (let col = 0; col < 16; col++) {
    const bit = lsb ? (word >> col) & 1 : (word >> (15 - col)) & 1;
    if (bit) {
      ctx.fillRect(ox + col * 2, oy, 2, 2);
    }
  }
}

function downloadCanvas(canvas, filename) {
  canvas.toBlob((blob) => {
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, "image/png");
}

// Exports miner.png (4-frame 128×32 strip, LSB-left convention) and npcs.png (palette-indexed
// RGBA sheet, 8 frames per row per sprite type), auto-migrating legacy u16 frames on export.
function exportSprites() {
  {
    const W = 128, H = 32;
    const cv = document.createElement("canvas");
    cv.width = W;
    cv.height = H;
    const ctx = cv.getContext("2d");
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = "#ffffff";
    MINER_SPRITE.forEach((frame, fi) => {
      frame.forEach((word, row) => renderSpriteRow(ctx, word, fi * 32, row * 2, true));
    });
    downloadCanvas(cv, "miner.png");
  }
  const sprites = state.sprites || [];
  if (sprites.length === 0) {
    alert("No npc sprites loaded — import a levels.json first.");
    return;
  }
  {
    const N = sprites.length;
    const W = 256, H = N * 32;
    const cv = document.createElement("canvas");
    cv.width = W; cv.height = H;
    const ctx = cv.getContext("2d");
    ctx.clearRect(0, 0, W, H);
    sprites.forEach((frames, ti) => {
      (frames || []).forEach((frame, fi) => {
        if (!frame) return;
        const isLegacy = frame.length !== 256;
        for (let row = 0; row < 16; row++) {
          for (let col = 0; col < 16; col++) {
            let idx;
            if (isLegacy) {
              const word = frame[row] || 0;
              idx = ((word >> (15 - col)) & 1) ? 7 : 0;
            } else {
              idx = frame[row * 16 + col] || 0;
            }
            if (idx === 0) continue;
            ctx.fillStyle = COLOURS[idx & 0xf];
            ctx.fillRect(fi * 32 + col * 2, ti * 32 + row * 2, 2, 2);
          }
        }
      });
    });
    downloadCanvas(cv, "npcs.png");
  }
}

// ── Sprite designer ───────────────────────────────────────────────────────────

// Migrate a legacy 1-bit u16 frame (16 u16 values) to 256 palette indices.
function migrateU16Frame(frame) {
  const out = new Array(256).fill(0);
  for (let row = 0; row < 16; row++) {
    const word = frame[row] || 0;
    for (let col = 0; col < 16; col++) {
      if ((word >> (15 - col)) & 1) out[row * 16 + col] = 7; // white
    }
  }
  return out;
}

// The game reads 16 u16 rows (bit 15 leftmost) for collision; colours live in npcs.png.
function packU16Frame(frame) {
  const out = [];
  for (let row = 0; row < 16; row++) {
    let word = 0;
    for (let col = 0; col < 16; col++) {
      if (frame[row * 16 + col]) word |= 1 << (15 - col);
    }
    out.push(word);
  }
  return out;
}

function getSpriteFrame(type, frame) {
  if (!state.sprites[type]) state.sprites[type] = [];
  if (!state.sprites[type][frame]) {
    state.sprites[type][frame] = new Array(256).fill(0);
  } else if (state.sprites[type][frame].length !== 256) {
    // Migrate legacy u16 format on first access
    state.sprites[type][frame] = migrateU16Frame(state.sprites[type][frame]);
  }
  return state.sprites[type][frame];
}

function refreshSpriteDesigner() {
  $("sprite-type-num").textContent  = spriteDesignerType;
  $("sprite-frame-num").textContent = spriteDesignerFrame;
  $("sprite-type-sel").value  = spriteDesignerType;
  $("sprite-frame-sel").value = spriteDesignerFrame;
  const data = getSpriteFrame(spriteDesignerType, spriteDesignerFrame);
  renderSpriteDesigner(spriteDesignerCanvas, data);
}

function setupSpriteDesignerEvents() {
  $("sprite-type-sel").addEventListener("change", (e) => {
    spriteDesignerType = Math.max(0, Math.min(28, +e.target.value));
    refreshSpriteDesigner();
  });
  $("sprite-frame-sel").addEventListener("change", (e) => {
    spriteDesignerFrame = Math.max(0, Math.min(7, +e.target.value));
    refreshSpriteDesigner();
  });
  $("sprite-ink-sel").addEventListener("change", (e) => {
    spriteDesignerInk = +e.target.value;
  });

  $("btn-sprite-clear").addEventListener("click", () => {
    getSpriteFrame(spriteDesignerType, spriteDesignerFrame).fill(0);
    refreshSpriteDesigner(); refresh();
  });
  $("btn-sprite-fill").addEventListener("click", () => {
    getSpriteFrame(spriteDesignerType, spriteDesignerFrame).fill(0xffff);
    refreshSpriteDesigner(); refresh();
  });
  $("btn-sprite-copy").addEventListener("click", () => {
    spriteDesignerClipboard = [...getSpriteFrame(spriteDesignerType, spriteDesignerFrame)];
  });
  $("btn-sprite-paste").addEventListener("click", () => {
    if (!spriteDesignerClipboard) return;
    const f = getSpriteFrame(spriteDesignerType, spriteDesignerFrame);
    spriteDesignerClipboard.forEach((v, i) => { f[i] = v; });
    refreshSpriteDesigner(); refresh();
  });
  $("btn-sprite-flip-h").addEventListener("click", () => {
    const f = getSpriteFrame(spriteDesignerType, spriteDesignerFrame);
    for (let r = 0; r < 16; r++) {
      let w = f[r], rev = 0;
      for (let b = 0; b < 16; b++) { rev = (rev << 1) | (w & 1); w >>= 1; }
      f[r] = rev;
    }
    refreshSpriteDesigner(); refresh();
  });
  $("btn-sprite-flip-v").addEventListener("click", () => {
    const f = getSpriteFrame(spriteDesignerType, spriteDesignerFrame);
    f.reverse();
    refreshSpriteDesigner(); refresh();
  });

  let spritePainting = false;
  let spriteBit = 1;

  function getSpritePixel(e) {
    const rect = spriteDesignerCanvas.getBoundingClientRect();
    const x = (e.clientX - rect.left) * (spriteDesignerCanvas.width / rect.width);
    const y = (e.clientY - rect.top)  * (spriteDesignerCanvas.height / rect.height);
    const ps = spriteDesignerCanvas.width / 16;
    const col = Math.max(0, Math.min(15, Math.floor(x / ps)));
    const row = Math.max(0, Math.min(15, Math.floor(y / ps)));
    return { col, row };
  }

  function paintSpritePixel(e) {
    const { col, row } = getSpritePixel(e);
    const f = getSpriteFrame(spriteDesignerType, spriteDesignerFrame);
    f[row * 16 + col] = spriteBit ? spriteDesignerInk : 0;
    refreshSpriteDesigner(); refresh();
  }

  spriteDesignerCanvas.addEventListener("mousedown", (e) => {
    e.preventDefault();
    spritePainting = true;
    const { col, row } = getSpritePixel(e);
    const f = getSpriteFrame(spriteDesignerType, spriteDesignerFrame);
    // Left-click: paint ink (or erase if ink=0); right-click: always erase
    spriteBit = (e.button === 2 || spriteDesignerInk === 0) ? 0 : 1;
    paintSpritePixel(e);
  });
  spriteDesignerCanvas.addEventListener("mousemove", (e) => {
    if (spritePainting) paintSpritePixel(e);
  });
  spriteDesignerCanvas.addEventListener("contextmenu", (e) => {
    e.preventDefault();
    spriteBit = 0; paintSpritePixel(e);
  });
  window.addEventListener("mouseup", () => { spritePainting = false; });

  refreshSpriteDesigner();
}

// ── Keyboard shortcuts ────────────────────────────────────────────────────────

function setupKeyboard() {
  document.addEventListener("keydown", (e) => {
    if (
      e.target.tagName === "INPUT" ||
      e.target.tagName === "SELECT" ||
      e.target.tagName === "TEXTAREA"
    )
      return;
    switch (e.key.toLowerCase()) {
      case "q":
        setTool("paint");
        break;
      case "w":
        setTool("erase");
        break;
      case "f":
        setTool("fill");
        break;
      case "r":
        setTool("npc");
        break;
      case "p":
        setTool("portal");
        break;
      case "s":
        setTool("start");
        break;
      case "+":
      case "=":
        setZoom(state.zoom + 1);
        break;
      case "-":
        setZoom(state.zoom - 1);
        break;
      case "escape":
        state.selectedNpc = -1;
        updatePropsPanel();
        refresh();
        break;
    }
  });
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function currentLevel() {
  return state.levels[state.currentLevel] || null;
}

function refresh() {
  renderLevel(levelCanvas, state);
}

function makeDefaultLevel() {
  const data = Array(512).fill(0);
  // Floor on bottom row, walls on sides
  for (let i = 0; i < GRID_W; i++) data[480 + i] = 1;
  for (let i = 0; i < GRID_H; i++) {
    data[i * GRID_W] = 1;
    data[i * GRID_W + GRID_W - 1] = 1;
  }
  return {
    name: `Level ${state.levels.length + 1}`,
    data,
    info: [
      { colour: 0x07, type: 3 }, // 0: space (black paper, white ink)
      { colour: 0x07, type: 4 }, // 1: solid (black paper, white ink)
      ...Array(8)
        .fill(null)
        .map(() => ({ colour: 0x07, type: 3 })),
    ],
    air: 8192,
    border: 0,
  };
}

function loadDefaultState() {
  $("zoom-label").textContent = state.zoom + "×";
  renderLevelList();
  renderGfxSlots();
  updateDesigner();
  refresh();
  // Try to auto-load ../levels.json when served from the editor directory
  fetch("../levels.json")
    .then((r) => (r.ok ? r.json() : null))
    .then((json) => {
      if (json) loadJSON(json);
    })
    .catch(() => {}); // silently ignore if not served or not found
  loadTileMasks("../tiles.png");
}

// Reads tiles.png (a 16-px row per level, a 16×16 cell per gfx slot) into per-slot masks of
// 256 flags, set where the pixel is opaque. The game draws set pixels in ink, the rest in paper.
function loadTileMasks(url) {
  const img = new Image();
  img.onload = () => {
    const cnv = document.createElement("canvas");
    cnv.width = img.width;
    cnv.height = img.height;
    const ctx = cnv.getContext("2d");
    ctx.drawImage(img, 0, 0);
    const px = ctx.getImageData(0, 0, img.width, img.height).data;
    state.tileMasks = [];
    for (let lv = 0; lv < Math.floor(img.height / 16); lv++) {
      const masks = [];
      for (let slot = 0; slot < Math.min(10, Math.floor(img.width / 16)); slot++) {
        const m = new Uint8Array(256);
        for (let i = 0; i < 256; i++) {
          const x = slot * 16 + (i % 16), y = lv * 16 + Math.floor(i / 16);
          m[i] = px[(y * img.width + x) * 4 + 3] >= 128 ? 1 : 0;
        }
        masks.push(m);
      }
      state.tileMasks.push(masks);
    }
    renderGfxSlots();
    updateDesigner();
    refresh();
  };
  img.src = url;
}
