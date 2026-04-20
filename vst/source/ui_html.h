#pragma once

// clang-format off
static const char kUIHTML[] = R"html(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
  width: 560px; min-height: 100px; overflow: hidden;
  background: #1a1a1e; color: #999;
  font-family: ui-monospace, "SF Mono", Menlo, monospace;
  user-select: none; -webkit-user-select: none;
  cursor: default;
}
#wrap { padding-bottom: 12px; }
header {
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 16px 0;
}
.title { font-size: 16px; font-weight: bold; color: #fff; }
hr { border: none; border-top: 1px solid #333; margin: 8px 16px; }

.col-headers {
  display: grid; grid-template-columns: 32px 1fr 1fr 1fr 1fr 1fr;
  gap: 2px; padding: 0 16px; margin-bottom: 2px;
}
.col-hdr { font-size: 8px; color: #555; padding-left: 4px; }

#gates { padding: 0 16px; display: flex; flex-direction: column; gap: 2px; }

.gate-row {
  display: grid; grid-template-columns: 32px 1fr 1fr 1fr 1fr 1fr;
  gap: 2px; height: 28px;
}
.gate-cell {
  border-radius: 3px; background: #2d2d2d;
  display: flex; align-items: center; justify-content: center;
  font-size: 10px;
}
.gate-num { font-size: 13px; font-weight: bold; color: #999; }
.gate-row.editing .gate-num { background: #e6802e; color: #000; }
.gate-ch, .gate-note { cursor: pointer; color: #999; }
.gate-ch:hover, .gate-note:hover { background: #383838; }
.gate-ch.editing, .gate-note.editing { background: #383838; border: 1px solid #555; }
.gate-note { }
.gate-mode { cursor: pointer; }
.gate-mode:hover { background: #383838; }
.gate-mode.velocity { color: #4db866; }
.gate-mode.pitch { color: #5b9bd5; }
.gate-mode.cc { color: #d5a05b; }
.gate-mode.off { color: #555; }
.gate-empty { border-radius: 3px; }
.gate-dac-ch { cursor: pointer; color: #999; }
.gate-dac-ch:hover { background: #383838; }
.gate-dac-ch.editing { background: #383838; border: 1px solid #555; }
.gate-dac-ch.pitch-val { color: #5b9bd5; }
.gate-dac-ch.cc-val { color: #d5a05b; }
.gate-cc-num { cursor: pointer; color: #d5a05b; }
.gate-cc-num:hover { background: #383838; }
.gate-cc-num.editing { background: #383838; border: 1px solid #555; }

#panel { padding: 12px 16px 8px; display: none; }
#panel.open { display: block; }
.panel-label { font-size: 9px; color: #555; margin-bottom: 4px; text-transform: uppercase; letter-spacing: 0.5px; }

.ch-grid {
  display: flex; gap: 2px; flex-wrap: wrap;
}
.ch-cell {
  width: 30px; height: 22px; border-radius: 3px;
  background: #2d2d2d; color: #888;
  display: flex; align-items: center; justify-content: center;
  font-size: 9px; cursor: pointer;
}
.ch-cell:last-child { width: 36px; }
.ch-cell:hover { background: #404040; }
.ch-cell.active { background: #e6802e; color: #000; }

.note-grid {
  display: grid; grid-template-columns: repeat(12, 1fr);
  gap: 1px;
}
.note-hdr {
  font-size: 8px; color: #555; display: flex;
  align-items: center; justify-content: center;
  height: 14px;
}
.note-cell {
  height: 20px; border-radius: 2px;
  background: #2d2d2d; color: #888;
  display: flex; align-items: center; justify-content: center;
  font-size: 8px; cursor: pointer;
}
.note-cell:hover { background: #404040; }
.note-cell.active { background: #e6802e; color: #000; }
.note-cell.any-cell { background: #333; color: #999; font-size: 9px; }
.note-cell.any-cell:hover { background: #404040; }
.note-cell.any-cell.active { background: #e6802e; color: #000; }
.note-cell.empty { visibility: hidden; }

.cc-grid {
  display: grid; grid-template-columns: repeat(16, 1fr);
  gap: 1px;
}
.cc-cell {
  height: 20px; border-radius: 2px;
  background: #2d2d2d; color: #888;
  display: flex; align-items: center; justify-content: center;
  font-size: 8px; cursor: pointer;
}
.cc-cell:hover { background: #404040; }
.cc-cell.active { background: #e6802e; color: #000; }

.popup-overlay {
  position: fixed; top: 0; left: 0; width: 100%; height: 100%; z-index: 99;
}
.popup {
  position: absolute; z-index: 100;
  background: #252528; border: 1px solid #444; border-radius: 4px;
  max-height: 240px; overflow-y: auto; min-width: 60px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.5);
}
.popup-item {
  padding: 4px 10px; font-size: 10px; color: #999;
  cursor: pointer; white-space: nowrap;
}
.popup-item:hover { background: #383838; }
.popup-item.active { background: #4db866; color: #000; }
</style>
</head>
<body>
<div id="wrap">
<header>
  <span class="title">tram8+</span>
  <span id="midi-port-cell" class="extra-cell" style="color:#999">
    <span id="midi-port-label">(none)</span>
  </span>
</header>
<hr>
<div class="col-headers">
  <div class="col-hdr">#</div>
  <div class="col-hdr">Ch</div>
  <div class="col-hdr">Note</div>
  <div class="col-hdr">DAC</div>
  <div class="col-hdr">DAC Ch</div>
  <div class="col-hdr">DAC Val</div>
</div>
<div id="gates"></div>
<div id="panel"></div>
<div id="popup-root"></div>
</div>

<script>
const NT = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
const MODES = ['Velocity','Pitch','CC','Off'];
const MODE_CLS = ['velocity','pitch','cc','off'];

function nName(n) { return n < 0 ? 'Any' : NT[n%12] + (Math.floor(n/12)-1); }
function nLabel(n) { return n < 0 ? 'Any' : nName(n) + ' (' + n + ')'; }
function chLbl(c) { return c < 0 ? 'Any' : '' + (c+1); }

// --- Popup (for MIDI port only) ---

function openPopup(anchor, items, active, onSelect) {
  closePopup();
  const r = anchor.getBoundingClientRect();
  const ov = document.createElement('div');
  ov.className = 'popup-overlay';
  ov.onclick = closePopup;
  const p = document.createElement('div');
  p.className = 'popup';
  p.style.left = r.left + 'px';
  p.style.top = r.bottom + 2 + 'px';
  let ae = null;
  items.forEach(it => {
    const el = document.createElement('div');
    el.className = 'popup-item' + (it.value === active ? ' active' : '');
    if (it.value === active) ae = el;
    el.textContent = it.label;
    el.onclick = e => { e.stopPropagation(); closePopup(); onSelect(it.value); };
    p.appendChild(el);
  });
  const root = document.getElementById('popup-root');
  root.appendChild(ov);
  root.appendChild(p);
  if (ae) setTimeout(() => ae.scrollIntoView({block:'center'}), 0);
}
function closePopup() { document.getElementById('popup-root').innerHTML = ''; }

// --- State ---

let midiPorts = [];
let editing = null;

const tram8 = {
  gates: Array.from({length: 8}, (_, i) => ({
    channel: -1, note: 60+i, mode: 0,
    dacChannel: -1, ccNum: 1
  })),

  setMidiPorts(ports) {
    midiPorts = ports;
    if (ports.length > 0) {
      document.getElementById('midi-port-label').textContent = ports[0];
      this.post({type:'setMidiPort', index:0});
    }
  },

  setGateState(gate, ch, note, mode, dacCh, ccN) {
    Object.assign(this.gates[gate], {channel:ch, note, mode,
      dacChannel: dacCh !== undefined ? dacCh : this.gates[gate].dacChannel,
      ccNum: ccN !== undefined ? ccN : this.gates[gate].ccNum});
    this.renderGates();
    this.renderPanel();
  },

  post(msg) {
    if (window.webkit?.messageHandlers?.tram8) {
      window.webkit.messageHandlers.tram8.postMessage(msg);
    }
  },

  requestResize() {
    if (this._rs) return;
    this._rs = true;
    setTimeout(() => {
      this._rs = false;
      const h = document.getElementById('wrap').offsetHeight;
      this.post({type:'resize', height: h});
    }, 50);
  },

  startEdit(gate, field) {
    if (editing && editing.gate === gate && editing.field === field) {
      editing = null;
    } else {
      editing = {gate, field};
    }
    this.renderGates();
    this.renderPanel();
  },

  cycleMode(i, e) {
    e.stopPropagation();
    this.gates[i].mode = (this.gates[i].mode + 1) % MODES.length;
    this.post({type:'setDacMode', gate:i, mode:this.gates[i].mode});
    this.renderGates();
    if (editing && editing.gate === i) this.renderPanel();
  },

  setValue(field, value) {
    if (!editing) return;
    const g = this.gates[editing.gate];
    const i = editing.gate;
    if (field === 'channel') { g.channel = value; this.post({type:'setChannel', gate:i, channel:value}); }
    if (field === 'note') { g.note = value; this.post({type:'setNote', gate:i, note:value}); }
    if (field === 'dacChannel') { g.dacChannel = value; this.post({type:'setDacChannel', gate:i, channel:value}); }
    if (field === 'ccNum') { g.ccNum = value; this.post({type:'setCcNum', gate:i, cc:value}); }
    this.renderGates();
    this.renderPanel();
  },

  renderGates() {
    const ct = document.getElementById('gates');
    ct.innerHTML = '';
    for (let i = 0; i < 8; i++) {
      const g = this.gates[i];
      const isEditing = editing && editing.gate === i;
      const row = document.createElement('div');
      row.className = 'gate-row' + (isEditing ? ' editing' : '');

      const num = document.createElement('div');
      num.className = 'gate-cell gate-num';
      num.textContent = i + 1;

      const ch = document.createElement('div');
      ch.className = 'gate-cell gate-ch' + (isEditing && editing.field === 'channel' ? ' editing' : '');
      ch.textContent = chLbl(g.channel);
      ch.onclick = e => { e.stopPropagation(); this.startEdit(i, 'channel'); };

      const note = document.createElement('div');
      note.className = 'gate-cell gate-note' + (isEditing && editing.field === 'note' ? ' editing' : '');
      note.textContent = nLabel(g.note);
      note.onclick = e => { e.stopPropagation(); this.startEdit(i, 'note'); };

      const mode = document.createElement('div');
      mode.className = 'gate-cell gate-mode ' + MODE_CLS[g.mode];
      mode.textContent = MODES[g.mode];
      mode.onclick = e => this.cycleMode(i, e);

      row.appendChild(num);
      row.appendChild(ch);
      row.appendChild(note);
      row.appendChild(mode);

      if (g.mode === 1) {
        const dc = document.createElement('div');
        dc.className = 'gate-cell gate-dac-ch pitch-val' + (isEditing && editing.field === 'dacChannel' ? ' editing' : '');
        dc.textContent = chLbl(g.dacChannel);
        dc.onclick = e => { e.stopPropagation(); this.startEdit(i, 'dacChannel'); };
        row.appendChild(dc);
        row.appendChild(document.createElement('div'));
      } else if (g.mode === 2) {
        const dc = document.createElement('div');
        dc.className = 'gate-cell gate-dac-ch cc-val' + (isEditing && editing.field === 'dacChannel' ? ' editing' : '');
        dc.textContent = chLbl(g.dacChannel);
        dc.onclick = e => { e.stopPropagation(); this.startEdit(i, 'dacChannel'); };
        const cc = document.createElement('div');
        cc.className = 'gate-cell gate-cc-num' + (isEditing && editing.field === 'ccNum' ? ' editing' : '');
        cc.textContent = g.ccNum;
        cc.onclick = e => { e.stopPropagation(); this.startEdit(i, 'ccNum'); };
        row.appendChild(dc);
        row.appendChild(cc);
      } else {
        row.appendChild(document.createElement('div'));
        row.appendChild(document.createElement('div'));
      }

      ct.appendChild(row);
    }
    this.requestResize();
  },

  renderPanel() {
    const panel = document.getElementById('panel');
    if (!editing) {
      panel.classList.remove('open');
      panel.innerHTML = '';
      this.requestResize();
      return;
    }
    panel.classList.add('open');
    panel.innerHTML = '';

    const g = this.gates[editing.gate];
    const field = editing.field;

    if (field === 'channel' || field === 'dacChannel') {
      const active = field === 'channel' ? g.channel : g.dacChannel;
      const lbl = document.createElement('div');
      lbl.className = 'panel-label';
      lbl.textContent = field === 'channel' ? 'Gate ' + (editing.gate+1) + ' -- Channel' : 'Gate ' + (editing.gate+1) + ' -- DAC Channel';
      panel.appendChild(lbl);

      const grid = document.createElement('div');
      grid.className = 'ch-grid';
      const labels = ['1','2','3','4','5','6','7','8','9','10','11','12','13','14','15','16','Any'];
      labels.forEach((l, idx) => {
        const ch = idx === 16 ? -1 : idx;
        const cell = document.createElement('div');
        cell.className = 'ch-cell' + (ch === active ? ' active' : '');
        cell.textContent = l;
        cell.onclick = () => this.setValue(field, ch);
        grid.appendChild(cell);
      });
      panel.appendChild(grid);
    }

    if (field === 'note') {
      const lbl = document.createElement('div');
      lbl.className = 'panel-label';
      lbl.textContent = 'Gate ' + (editing.gate+1) + ' -- Note';
      panel.appendChild(lbl);

      const grid = document.createElement('div');
      grid.className = 'note-grid';

      for (let c = 0; c < 12; c++) {
        const h = document.createElement('div');
        h.className = 'note-hdr';
        h.textContent = NT[c];
        grid.appendChild(h);
      }
      for (let r = 0; r < 11; r++) {
        for (let c = 0; c < 12; c++) {
          const n = r * 12 + c;
          const cell = document.createElement('div');
          if (n < 128) {
            cell.className = 'note-cell' + (n === g.note ? ' active' : '');
            cell.textContent = n;
            cell.onclick = () => this.setValue('note', n);
          } else if (n === 128) {
            cell.className = 'note-cell any-cell' + (g.note === -1 ? ' active' : '');
            cell.textContent = 'Any';
            cell.onclick = () => this.setValue('note', -1);
          } else {
            cell.className = 'note-cell empty';
          }
          grid.appendChild(cell);
        }
      }
      panel.appendChild(grid);
    }

    if (field === 'ccNum') {
      const lbl = document.createElement('div');
      lbl.className = 'panel-label';
      lbl.textContent = 'Gate ' + (editing.gate+1) + ' -- CC Number';
      panel.appendChild(lbl);

      const grid = document.createElement('div');
      grid.className = 'cc-grid';
      for (let cc = 0; cc < 128; cc++) {
        const cell = document.createElement('div');
        cell.className = 'cc-cell' + (cc === g.ccNum ? ' active' : '');
        cell.textContent = cc;
        cell.onclick = () => this.setValue('ccNum', cc);
        grid.appendChild(cell);
      }
      panel.appendChild(grid);
    }

    this.requestResize();
  },

  init() {
    document.getElementById('midi-port-cell').onclick = e => {
      const items = [{label:'(none)', value:-1}];
      midiPorts.forEach((n,i) => items.push({label:n, value:i}));
      openPopup(e.currentTarget, items, -1, idx => {
        document.getElementById('midi-port-label').textContent = idx < 0 ? '(none)' : midiPorts[idx];
        this.post({type:'setMidiPort', index:idx});
      });
    };
    this.renderGates();
    this.post({type:'ready'});
  }
};

document.addEventListener('DOMContentLoaded', () => tram8.init());
</script>
</body>
</html>)html";
// clang-format on
