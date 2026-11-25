# Phù thủy nhảy mây (Three.js)

Game 3D: phù thủy đứng trên các đám mây, nhấn Space để nhảy và dùng chuột để điều khiển hướng bay và lực rơi.

## Cách chạy

1. Tạo file `witch.html` và dán toàn bộ code ở phần dưới vào.
2. Lưu ý: nếu dùng Notepad, chọn **Save as type: All Files**. Mac dùng TextEdit ở chế độ **Plain Text**.
3. Nhấp đúp vào `witch.html` để mở bằng Chrome, Edge hoặc Firefox.
4. Máy cần có internet vì game tải Three.js từ CDN.

## Cách chơi

- **Space** hoặc **chuột trái** khi đứng trên mây: nhảy lên và bay về phía trước.
- **Di chuột trái/phải** khi đang bay: điều khiển hướng bay.
- **Di chuột lên/xuống** khi đang bay: điều khiển lực rơi. Chuột ở trên thì rơi chậm và bay xa hơn, chuột ở dưới thì lao nhanh.
- Rơi trúng mây thì đứng lại. Rơi mạnh thì phù thủy nảy lên vài lần.
- Rơi xuống biển là thua. Nhấn Space để chơi lại.

## Code

```html
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phù thủy nhảy mây</title>
<style>
  html, body { margin:0; height:100%; overflow:hidden; background:#2b3a7a; font-family:sans-serif; color:#fff; user-select:none; }
  #hud { position:absolute; top:12px; left:12px; font-size:18px; text-shadow:0 0 4px #000; pointer-events:none; }
  #bar { width:200px; height:12px; background:#333a; border-radius:6px; overflow:hidden; margin-top:4px; }
  #energy { height:100%; width:0%; background:linear-gradient(90deg,#a68cff,#ffd23f); }
  #msg { position:absolute; inset:0; display:flex; flex-direction:column; align-items:center; justify-content:center; font-size:28px; text-align:center; text-shadow:0 0 8px #000; pointer-events:none; }
  #msg small { font-size:16px; opacity:.85; }
</style>
</head>
<body>
<div id="hud">
  Mây đã nhảy: <span id="score">0</span> · Độ cao: <span id="alt">0</span>m
  <div style="font-size:14px;margin-top:6px">Lực rơi (chuột lên/xuống)</div>
  <div id="bar"><div id="energy"></div></div>
</div>
<div id="msg">Nhấn SPACE để nhảy lên mây đầu tiên<br><small>Space: nhảy khi đứng trên mây · Di chuột trái/phải: hướng bay · Di chuột lên/xuống: lực rơi</small></div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
<script>
// ===== Tiện ích =====
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const rnd = (a, b) => a + Math.random() * (b - a);
const JUMP = 11;

// ===== Scene, camera, renderer =====
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x2b3a7a);
scene.fog = new THREE.Fog(0x2b3a7a, 60, 230);

const camera = new THREE.PerspectiveCamera(70, innerWidth / innerHeight, 0.1, 400);
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
document.body.appendChild(renderer.domElement);

addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});

// ===== Ánh sáng =====
scene.add(new THREE.HemisphereLight(0xcfe0ff, 0x2a2f55, 1.0));
const moonLight = new THREE.DirectionalLight(0xffffff, 0.6);
moonLight.position.set(-30, 60, -20);
scene.add(moonLight);

// ===== Trăng, sao, biển xa =====
const moon = new THREE.Mesh(
  new THREE.SphereGeometry(8, 24, 16),
  new THREE.MeshBasicMaterial({ color: 0xfff6d5, fog: false })
);
scene.add(moon);

const starGeo = new THREE.BufferGeometry();
const starPos = [];
for (let i = 0; i < 800; i++) starPos.push(rnd(-250, 250), rnd(10, 200), rnd(-300, 100));
starGeo.setAttribute('position', new THREE.Float32BufferAttribute(starPos, 3));
const stars = new THREE.Points(starGeo, new THREE.PointsMaterial({ color: 0xffffff, size: 1.3, fog: false }));
scene.add(stars);

const sea = new THREE.Mesh(
  new THREE.PlaneGeometry(1200, 1200),
  new THREE.MeshLambertMaterial({ color: 0x1d2c5a })
);
sea.rotation.x = -Math.PI / 2;
sea.position.y = -30;
scene.add(sea);

// ===== Phù thủy =====
const witch = new THREE.Group();
scene.add(witch);
const M = c => new THREE.MeshLambertMaterial({ color: c });
function part(geo, mat, x, y, z, rx = 0, ry = 0, rz = 0) {
  const m = new THREE.Mesh(geo, mat);
  m.position.set(x, y, z);
  m.rotation.set(rx, ry, rz);
  witch.add(m);
  return m;
}
// Váy, thân, đầu, tóc
part(new THREE.ConeGeometry(0.55, 1.4, 12), M(0x6a2c91), 0, 0.7, 0);
part(new THREE.CylinderGeometry(0.35, 0.4, 1.0, 10), M(0x7b3fa8), 0, 1.5, 0);
part(new THREE.SphereGeometry(0.33, 16, 12), M(0xf5c9a5), 0, 2.2, 0);
part(new THREE.SphereGeometry(0.36, 16, 12), M(0x2b1b3d), 0, 2.25, 0.12);
// Nón
part(new THREE.CylinderGeometry(0.85, 0.85, 0.08, 20), M(0x1a1a2e), 0, 2.6, 0);
part(new THREE.ConeGeometry(0.45, 1.3, 12), M(0x1a1a2e), 0, 3.25, 0);
// Tay
part(new THREE.CylinderGeometry(0.08, 0.08, 0.9, 6), M(0x7b3fa8), -0.4, 1.7, -0.35, -1.1);
part(new THREE.CylinderGeometry(0.08, 0.08, 0.9, 6), M(0x7b3fa8),  0.4, 1.7, -0.35, -1.1);
// Chổi
part(new THREE.CylinderGeometry(0.06, 0.06, 3.4, 8), M(0x8b5a2b), 0, 0.25, 0, Math.PI / 2);
part(new THREE.ConeGeometry(0.35, 1.0, 8), M(0xc9a227), 0, 0.25, 1.9, -Math.PI / 2);

// ===== Mây =====
const cloudMat = new THREE.MeshLambertMaterial({ color: 0xffffff, emissive: 0x8a96bb, emissiveIntensity: 0.3 });
const cloudGeos = [0, 1, 2].map(() => new THREE.SphereGeometry(rnd(1.1, 1.5), 14, 10));
const clouds = [];
let nextId = 0;

function addCloud(x, y, z) {
  const g = new THREE.Group();
  const n = 5 + Math.floor(Math.random() * 3);
  for (let i = 0; i < n; i++) {
    const m = new THREE.Mesh(cloudGeos[i % 3], cloudMat);
    m.position.set(rnd(-2.4, 2.4), rnd(-0.8, -0.2), rnd(-1.4, 1.4));
    g.add(m);
  }
  g.position.set(x, y, z);
  scene.add(g);
  const c = { id: nextId++, mesh: g, x, y, z, top: y + 1.2, hw: 2.6, hd: 2.0 };
  clouds.push(c);
  return c;
}

// Tạo thêm mây phía trước, xóa mây đã bay qua phía sau
function extendPath() {
  while (clouds[clouds.length - 1].z > witch.position.z - 160) {
    const p = clouds[clouds.length - 1];
    addCloud(
      clamp(p.x + rnd(-7, 7), -14, 14),
      clamp(p.y + rnd(-1.2, 1.6), -1, 6),
      p.z - rnd(8, 10)
    );
  }
  while (clouds.length > 1 && clouds[0].z > witch.position.z + 40) {
    scene.remove(clouds[0].mesh);
    clouds.shift();
  }
}

// ===== Trạng thái =====
let state = 'ready';       // ready | play | over
let score = 0;
let grounded = true;
let curTop = 0;
let vx = 0, vy = 0, vz = 0;
let bounces = 0;
let overAt = 0;
let fallG = 19;
const mouse = { x: 0.5, y: 0.5 };
const msg = document.getElementById('msg');
const scoreEl = document.getElementById('score');
const altEl = document.getElementById('alt');
const energyEl = document.getElementById('energy');

function showMsg(html) {
  msg.innerHTML = html;
  msg.style.display = 'flex';
}

function reset() {
  for (const c of clouds) scene.remove(c.mesh);
  clouds.length = 0;
  nextId = 0;
  const first = addCloud(0, 0, 0);
  witch.position.set(0, first.top, 0);
  witch.rotation.set(0, 0, 0);
  curTop = first.top;
  extendPath();
  vx = 0; vy = 0; vz = 0;
  grounded = true;
  bounces = 0;
  score = 0;
  state = 'ready';
  showMsg('Nhấn SPACE để nhảy lên mây đầu tiên<br><small>Space: nhảy khi đứng trên mây · Di chuột trái/phải: hướng bay · Di chuột lên/xuống: lực rơi</small>');
}

function jump() {
  if (!grounded) return;
  grounded = false;
  vy = JUMP;
  vz = -9;
  vx = (mouse.x - 0.5) * 24;
  bounces = 0;
}

function land(c) {
  const impact = -vy;
  witch.position.y = c.top;
  curTop = c.top;
  score = Math.max(score, c.id);
  if (impact > 8 && bounces < 3) {
    // Rơi mạnh thì nảy lên, giảm lực và dừng tiến
    vy = impact * 0.4;
    vz = 0;
    vx *= 0.3;
    bounces++;
  } else {
    grounded = true;
    vx = 0; vy = 0; vz = 0;
    bounces = 0;
  }
}

function crash() {
  state = 'over';
  overAt = performance.now();
  showMsg('Rơi xuống biển rồi!<br><small>Mây đã nhảy: ' + score + '<br>Nhấn SPACE để chơi lại</small>');
}

function action() {
  if (state === 'over') {
    if (performance.now() - overAt < 800) return;
    reset();
  }
  if (state === 'ready') {
    state = 'play';
    msg.style.display = 'none';
  }
  jump();
}

// ===== Điều khiển =====
addEventListener('keydown', e => {
  if (e.code === 'Space') {
    e.preventDefault();
    if (!e.repeat) action();
  }
});
addEventListener('pointerdown', e => {
  if (e.button === 0) action();
});
addEventListener('pointermove', e => {
  mouse.x = e.clientX / innerWidth;
  mouse.y = e.clientY / innerHeight;
});

// ===== Cập nhật =====
function update(dt, t) {
  if (!grounded) {
    // Chuột ở trên: rơi chậm, bay xa. Chuột ở dưới: lao nhanh.
    fallG = 8 + mouse.y * 22;
    vy -= fallG * dt;

    // Chuột trái/phải: điều khiển hướng bay
    const targetVx = (mouse.x - 0.5) * 24;
    vx += (targetVx - vx) * Math.min(1, 5 * dt);
    witch.position.x = clamp(witch.position.x + vx * dt, -30, 30);
    witch.position.z += vz * dt;

    const prevY = witch.position.y;
    witch.position.y += vy * dt;

    // Kiểm tra rơi trúng mây
    if (state === 'play' && vy < 0) {
      for (const c of clouds) {
        if (prevY >= c.top && witch.position.y <= c.top &&
            Math.abs(witch.position.x - c.x) < c.hw &&
            Math.abs(witch.position.z - c.z) < c.hd) {
          land(c);
          break;
        }
      }
    }

    if (!grounded) {
      witch.rotation.x = clamp(-vy * 0.03, -0.6, 0.6);
      witch.rotation.z = clamp(-vx * 0.03, -0.6, 0.6);
    }

    if (state === 'play' && witch.position.y < -20) crash();
  } else {
    witch.rotation.x = 0;
    witch.rotation.z = 0;
    witch.position.y = curTop;
    fallG = 19;
  }

  if (grounded) {
    witch.position.y = curTop;
  }

  extendPath();

  // Môi trường theo phù thủy
  sea.position.z = witch.position.z;
  stars.position.set(witch.position.x * 0.3, 0, witch.position.z);
  moon.position.set(witch.position.x - 50, 80, witch.position.z - 200);

  // Camera bám sau phù thủy
  const want = new THREE.Vector3(witch.position.x * 0.6, witch.position.y + 4.5, witch.position.z + 11);
  camera.position.lerp(want, 0.12);
  camera.lookAt(witch.position.x * 0.6, witch.position.y + 1.5, witch.position.z - 6);

  // HUD
  scoreEl.textContent = score;
  altEl.textContent = Math.max(0, Math.round(witch.position.y));
  energyEl.style.width = (clamp((fallG - 8) / 22, 0, 1) * 100) + '%';
}

// ===== Vòng lặp =====
reset();
let last = performance.now();
let T = 0;
function loop(now) {
  const dt = Math.min(0.05, (now - last) / 1000);
  last = now;
  T += dt;
  update(dt, T);
  renderer.render(scene, camera);
  requestAnimationFrame(loop);
}
requestAnimationFrame(loop);
</script>
</body>
</html>
```

## Chỉnh độ khó

- **Khoảng cách giữa mây:** dòng `p.z - rnd(8, 10)`. Tăng số để khó hơn.
- **Kích thước mây:** `hw` và `hd` trong hàm `addCloud`. Tăng số để dễ đáp hơn.
- **Lực nhảy:** hằng số `JUMP`.
- **Số lần nảy tối đa:** `bounces < 3`.
