// Aculyre promo: three pieces in one Full HD film, after Respira's promo.mjs.
// The instrument's own strings are the picture: one horizontal line per
// string of the lyre playing, bass at the bottom; the performed notes ride in
// along their string from the right and are plucked at the pluck line. Frames
// are a deterministic function of time and the timeline (notes from the
// performance; each string's energy and state from the engine's read-only
// 60 Hz observers, AculyreEngine::stringActivity). Brightness follows the
// measured energy (dB); the strings' visible sway is illustrative: its size
// follows that energy, its rate is not the pitch. Gold strings were plucked;
// teal ones nobody plucked and ring only in sympathy.
// usage: node promo.mjs <timeline.json> <audio.wav> <out.mp4> [--portrait] [--still t1 t2 ...]
// --portrait draws a 1080x1920 short, every word inside the project's TikTok
// safe box (x 130-821, y 307-1190); the strings and progress bar are artwork
// and run below and behind it.
import fs from 'node:fs';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const {createCanvas, loadImage, GlobalFonts} = require('@napi-rs/canvas');
const A = path.resolve('../assets');
GlobalFonts.registerFromPath(`${A}/LibreBaskerville-wght.ttf`, 'Bask');
GlobalFonts.registerFromPath('/usr/share/fonts/opentype/inter/Inter-Regular.otf', 'InterR');
GlobalFonts.registerFromPath('/usr/share/fonts/opentype/inter/Inter-Medium.otf', 'InterM');
GlobalFonts.registerFromPath('/usr/share/fonts/opentype/inter/Inter-SemiBold.otf', 'InterS');
const logo = await loadImage(`${A}/logo.png`);

let [timelinePath, audioPath, outPath, ...rest] = process.argv.slice(2);
const P = rest[0] === '--portrait'; if (P) rest = rest.slice(1);
const W = P ? 1080 : 1920, H = P ? 1920 : 1080, MX = 475;   // MX: the safe box's centre line
const still = rest[0] === '--still' ? rest.slice(1).map(Number) : null;
const T = JSON.parse(fs.readFileSync(timelinePath));
const fps = T.fps, dur = T.duration, frames = Math.ceil(dur * fps), SEG = T.segments;
const clamp = (x, a = 0, b = 1) => Math.min(b, Math.max(a, x));
const ease = x => { x = clamp(x); return x * x * (3 - 2 * x); };
const GOLD = [236, 184, 92], TEAL = [104, 204, 190], IVORY = [246, 236, 218];
const rgba = (c, a) => `rgba(${c[0]},${c[1]},${c[2]},${clamp(a)})`;
const mixc = (a, b, f) => a.map((x, i) => Math.round(x + (b[i] - x) * f));
// Pieces change through a dip at the middle of their audio overlap, so two titles
// never sit on top of each other; the first piece fades in from its start.
const XF = 0.35;
SEG.forEach((s, i) => {
  s.in = i ? (s.start + SEG[i - 1].end) / 2 : s.start;
  s.out = i < SEG.length - 1 ? (s.end + SEG[i + 1].start) / 2 : Infinity;
  const span = s.end - s.start;
  s.shown = s.notes.filter(n => n[0] >= s.src0 - 0.02 && n[0] <= s.src0 + span);
  s.last = Math.max(...s.shown.map(n => n[1])) - s.src0 + s.start;   // promo time of the last release
  // the string each note plucks: its own, or with the levers the one a semitone below
  s.row = p => { const k = s.strings.indexOf(p); if (k >= 0) return [k, false];
    const j = s.strings.indexOf(p - 1); if (j >= 0) return [j, true];
    return [p < s.strings[0] ? 0 : s.strings.length - 1, false]; };
});
const segAlpha = (s, t) => ease((t - s.in) / XF) * ease((s.out - t) / XF);
const final = SEG[SEG.length - 1], OUT0 = final.last + 0.2;   // the end card rises as the last chord rings
// the string field: x0 the bridge, x1 the tuning pins; the pluck line at the patch's 27 % from the bridge
const R = P ? {x0: 40, x1: 1040, y0: 860, y1: 1640} : {x0: 960, x1: 1784, y0: 160, y1: 900};
R.play = R.x0 + 0.27 * (R.x1 - R.x0);
const PPS = P ? 190 : 175;   // pixels per second of the notes riding in

function text(ctx, str, x, y, font, color, align = 'left', glow = null, sp = 0) {
  ctx.save(); ctx.font = font; ctx.fillStyle = color; ctx.textAlign = align;
  if (sp) ctx.letterSpacing = `${sp}px`;
  if (glow) { ctx.shadowColor = glow[0]; ctx.shadowBlur = glow[1]; }
  ctx.fillText(str, x, y); ctx.restore();
}
function round(ctx, x, y, w, h, r) {
  r = Math.min(r, h / 2, w / 2); ctx.beginPath(); ctx.moveTo(x + r, y); ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r); ctx.arcTo(x, y + h, x, y, r); ctx.arcTo(x, y, x + w, y, r); ctx.closePath();
}
const frameAt = (s, u) => clamp(Math.round(u * 60) - 1, 0, s.energy.length - 1);

// one piece's scene at piece time u, drawn at opacity a
function scene(ctx, s, u, a) {
  const n = s.strings.length, gap = (R.y1 - R.y0) / n, f = frameAt(s, u);
  const yOf = k => R.y1 - (k + 0.5) * gap;
  ctx.save(); ctx.globalAlpha = a;
  // the bridge bar and the pin rail, as planar lines
  ctx.fillStyle = 'rgba(232,196,140,0.30)'; round(ctx, R.x0 - 10, R.y0 - 6, 6, R.y1 - R.y0 + 12, 3); ctx.fill();
  ctx.fillStyle = 'rgba(232,196,140,0.18)'; round(ctx, R.x1 + 4, R.y0 - 6, 4, R.y1 - R.y0 + 12, 2); ctx.fill();
  let plucked = 0, sympathy = 0;
  for (let k = 0; k < n; k++) {
    const y = yOf(k), dB = s.energy[f][k], st = s.state[f][k];
    const lit = clamp((dB + 62) / 56);   // -62 dB dark .. -6 dB full
    if (st > 0 && dB > -60) plucked++; else if (st === 0 && dB > -50) sympathy++;
    const c = st > 0 ? GOLD : TEAL, base = st > 0 || dB > -50 ? c : [200, 170, 130];
    const amp = (P ? 15 : 13) * Math.pow(lit, 1.6), ph = Math.sin(2 * Math.PI * (2.1 + (k % 7) * 0.37) * u + k * 2.3);
    const wid = 1.2 + 1.6 * (1 - k / n);   // the bass strings drawn heavier
    for (const [lw, al] of [[wid + 6, 0.10 + 0.30 * lit], [wid, 0.22 + 0.78 * lit]]) {
      ctx.beginPath(); ctx.moveTo(R.x0, y);
      ctx.quadraticCurveTo((R.x0 + R.x1) / 2, y + 2 * amp * ph, R.x1, y);   // the vertex sits at amp
      ctx.strokeStyle = rgba(lit > 0.02 ? mixc(base, IVORY, 0.25 * lit) : [150, 120, 92], al); ctx.lineWidth = lw; ctx.stroke();
    }
  }
  // the notes riding in on their string, plucked at the line
  const ahead = (R.x1 - R.play) / PPS, barH = Math.min(14, gap * 0.5);
  for (const nt of s.shown) {
    const [st, e, p] = nt;
    if (st > u + ahead || st < u - 0.25) continue;
    const [k, raised] = s.row(p), y = yOf(k);
    const xs = R.play + (st - u) * PPS, len = Math.max(barH, Math.min((e - st) * PPS, R.x1 - xs));
    const hit = clamp(1 - (u - st) / 0.25), edge = clamp((R.x1 - xs) / 60);
    ctx.save(); ctx.globalAlpha *= (st <= u ? hit : 0.85) * edge;
    if (st <= u) { ctx.shadowColor = rgba(GOLD, 0.9); ctx.shadowBlur = 18; }
    ctx.fillStyle = rgba(st <= u ? IVORY : GOLD, 1);
    round(ctx, Math.max(R.play, xs), y - barH / 2, st <= u ? barH : Math.min(len, R.x1 - xs), barH, barH / 2); ctx.fill();
    if (raised) { ctx.fillStyle = rgba(IVORY, 0.9); ctx.beginPath(); ctx.arc(R.x1 + 18, y, 3.5, 0, Math.PI * 2); ctx.fill(); }
    ctx.restore();
  }
  // the piece and its lyre
  const who = `${s.patch.toUpperCase()}  ·  ${s.how}`;
  const live = `${plucked} plucked  ·  ${sympathy} in sympathy`;
  if (P) {
    text(ctx, s.title, MX, 700, '84px Bask', '#f6ecdc', 'center', ['rgba(255,200,140,0.25)', 26]);
    text(ctx, s.sub, MX, 752, '30px InterS', '#e9b95e', 'center');
    text(ctx, s.patch.toUpperCase(), MX, 800, '24px InterS', '#eadfcf', 'center', null, 2);
  } else {
    text(ctx, s.title, 136, 640, '96px Bask', '#f6ecdc', 'left', ['rgba(255,200,140,0.25)', 26]);
    text(ctx, s.sub, 140, 700, '38px InterS', '#e9b95e');
    text(ctx, who, 140, 790, '25px InterS', '#eadfcf', 'left', null, 1.5);
    ctx.save(); ctx.font = '25px InterR'; ctx.fillStyle = '#d9cbb6'; ctx.fillText(`${plucked} plucked  ·  `, 140, 834);
    const w0 = ctx.measureText(`${plucked} plucked  ·  `).width; ctx.fillStyle = rgba(TEAL, 1);
    ctx.fillText(`${sympathy} ringing in sympathy`, 140 + w0, 834); ctx.restore();
  }
  ctx.restore();
}

function draw(ctx, t) {
  const fade = ease(t / 0.5) * ease((dur - t) / 0.6);
  const live = SEG.map(s => [s, segAlpha(s, t)]).filter(([, a]) => a > 0);
  const endA = ease((t - OUT0) / 0.9);
  // ground: dark walnut, a warm glow behind the strings that rises with their energy
  const g = ctx.createLinearGradient(0, 0, W, H);
  g.addColorStop(0, '#120905'); g.addColorStop(0.5, '#21120a'); g.addColorStop(1, '#0d0603');
  ctx.fillStyle = g; ctx.fillRect(0, 0, W, H);
  let energy = 0;
  for (const [s, a] of live) { const f = frameAt(s, t - s.start + s.src0); energy += a * clamp((Math.max(...s.energy[f]) + 50) / 45); }
  const cx = (R.x0 + R.x1) / 2, cy = (R.y0 + R.y1) / 2;
  const glow = ctx.createRadialGradient(cx, cy, 0, cx, cy, P ? 640 : 720);
  glow.addColorStop(0, `rgba(150,92,34,${(0.22 + 0.10 * clamp(energy)) * fade})`);
  glow.addColorStop(0.6, 'rgba(90,50,20,0.08)'); glow.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.fillStyle = glow; ctx.fillRect(0, 0, W, H);
  for (let i = 0; i < 30; i++) {   // slow dust, deterministic
    const x = ((i * 397.31 + t * (5 + (i % 4) * 3)) % (W + 80)) - 40, y = (i * 211.7 + Math.sin(t * 0.19 + i) * 24 + H) % H;
    const r = 2 + (i % 5) * 1.3, a = (0.03 + (i % 3) * 0.022) * fade;
    const d = ctx.createRadialGradient(x, y, 0, x, y, r * 4);
    d.addColorStop(0, `rgba(255,222,170,${a})`); d.addColorStop(1, 'rgba(255,222,170,0)');
    ctx.fillStyle = d; ctx.fillRect(x - r * 4, y - r * 4, r * 8, r * 8);
  }
  for (const [s, a] of live) scene(ctx, s, t - s.start + s.src0, a * fade * (s === final ? 1 - endA : 1));
  ctx.fillStyle = `rgba(255,246,232,${0.7 * fade * (1 - endA)})`; ctx.fillRect(R.play - 1, R.y0 - 20, 2, R.y1 - R.y0 + 40);

  // the end card, over the strings
  if (endA > 0) {
    ctx.save(); ctx.globalAlpha = endA * fade; const mx = P ? MX : (R.x0 + R.x1) / 2, f = P ? 72 : 96;
    const y = P ? 1030 : 470, dy = P ? 92 : 120;
    text(ctx, 'No samples.', mx, y, `${f}px Bask`, '#f6ecdc', 'center', ['rgba(255,200,140,0.25)', 26]);
    text(ctx, 'Just strings.', mx, y + dy, `${f}px Bask`, '#f0c977', 'center', ['rgba(255,200,140,0.3)', 26]);
    if (!P) text(ctx, 'Three lyres · one physical model', mx, y + dy + 72, '36px InterS', '#e9b95e', 'center');
    ctx.restore();
  }

  // identity: on every frame, never faded
  if (P) {
    ctx.font = '28px InterS'; ctx.letterSpacing = '8px'; const pw = ctx.measureText('PROTOCODUS').width; ctx.letterSpacing = '0px';
    const lx = MX - (72 + 18 + pw) / 2;
    ctx.drawImage(logo, lx, 320, 72, 72); text(ctx, 'PROTOCODUS', lx + 90, 367, '28px InterS', '#efe4d4', 'left', null, 8);
  } else {
    ctx.drawImage(logo, 140, 108, 88, 88); text(ctx, 'PROTOCODUS', 248, 164, '30px InterS', '#efe4d4', 'left', null, 9);
  }
  const rx = P ? MX : 136, ry = P ? 492 : 324, rs = P ? 104 : 136;
  ctx.save(); ctx.font = `${rs}px Bask`; ctx.textAlign = P ? 'center' : 'left';
  const gg = ctx.createLinearGradient(0, ry - rs * 0.8, 0, ry + rs * 0.15);
  gg.addColorStop(0, '#f8dea0'); gg.addColorStop(0.55, '#e7b660'); gg.addColorStop(1, '#c48a3a');
  ctx.shadowColor = 'rgba(255,190,100,0.35)'; ctx.shadowBlur = 32; ctx.fillStyle = gg; ctx.fillText('Aculyre', rx, ry); ctx.restore();
  text(ctx, 'physically modelled lyre', P ? MX : 140, P ? 552 : 400, `${P ? 28 : 34}px InterM`, '#eadfd0', P ? 'center' : 'left', null, 1.2);
  text(ctx, `Reason Rack Extension · ${T.version}`, P ? MX : 140, P ? 588 : 446, `${P ? 22 : 25}px InterM`, 'rgba(222,190,124,0.92)', P ? 'center' : 'left', null, 0.6);

  // progress across the whole film, one track per piece
  const [px0, px1, py] = P ? [140, 940, 1700] : [140, 1784, 985], gp = 14;
  const bounds = SEG.map((s, i) => [i ? s.in : 0, Math.min(s.out, dur)]);
  ctx.save(); ctx.globalAlpha = fade;
  for (const [b0, b1] of bounds) {
    const x0 = px0 + (px1 - px0) * b0 / dur + (b0 > 0 ? gp / 2 : 0), x1 = px0 + (px1 - px0) * b1 / dur - (b1 < dur ? gp / 2 : 0);
    ctx.fillStyle = 'rgba(255,255,255,0.8)'; round(ctx, x0, py, x1 - x0, 5, 2.5); ctx.fill();
    const fr = clamp((t - b0) / (b1 - b0));
    if (fr > 0) { ctx.fillStyle = '#e8b55a'; round(ctx, x0, py, Math.max(5, (x1 - x0) * fr), 5, 2.5); ctx.fill(); }
  }
  ctx.restore();
}

const canvas = createCanvas(W, H), ctx = canvas.getContext('2d');
if (still) {
  for (const t of still) { ctx.clearRect(0, 0, W, H); draw(ctx, t); fs.writeFileSync(`${outPath}-${t}.png`, canvas.toBuffer('image/png')); }
} else {
  const ff = spawn('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', '-f', 'rawvideo', '-pixel_format', 'rgba',
    '-video_size', `${W}x${H}`, '-framerate', String(fps), '-i', 'pipe:0', '-i', audioPath, '-map', '0:v', '-map', '1:a',
    '-c:v', 'libx264', '-preset', 'medium', '-crf', '18', '-profile:v', 'high', '-pix_fmt', 'yuv420p',
    '-colorspace', 'bt709', '-color_primaries', 'bt709', '-color_trc', 'bt709', '-color_range', 'tv',
    '-c:a', 'aac', '-b:a', '384k', '-ar', '48000', '-af', 'volume=-0.3dB', '-shortest', '-movflags', '+faststart', outPath],
    {stdio: ['pipe', 'inherit', 'inherit']});
  for (let i = 0; i < frames; i++) {
    draw(ctx, (i + 0.5) / fps);
    const buf = Buffer.from(ctx.getImageData(0, 0, W, H).data.buffer);
    if (!ff.stdin.write(buf)) await new Promise(r => ff.stdin.once('drain', r));
    if (i % 300 === 0) process.stderr.write(`${path.basename(outPath)} ${i}/${frames}\n`);
  }
  ff.stdin.end(); await new Promise(r => ff.on('close', r));
}
