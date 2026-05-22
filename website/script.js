/* ── Nav scroll state ─────────────────────────────────────────────── */
const nav = document.getElementById('nav');
window.addEventListener('scroll', () => {
  nav.classList.toggle('scrolled', window.scrollY > 10);
}, { passive: true });

/* ── Animated waveform canvas ─────────────────────────────────────── */
(function () {
  const canvas = document.getElementById('waveform-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');

  // Line config — multiple sine waves with different frequencies and phases
  const lines = [
    { freq: 0.012, amp: 0.06, speed: 0.4,  phase: 0,    color: 'rgba(255,94,20,0.7)',  width: 1.5 },
    { freq: 0.008, amp: 0.09, speed: 0.25, phase: 1.2,  color: 'rgba(255,94,20,0.4)',  width: 1.0 },
    { freq: 0.018, amp: 0.04, speed: 0.6,  phase: 2.5,  color: 'rgba(255,255,255,0.12)', width: 0.8 },
    { freq: 0.006, amp: 0.12, speed: 0.15, phase: 4.0,  color: 'rgba(255,94,20,0.2)',  width: 2.0 },
    { freq: 0.022, amp: 0.03, speed: 0.8,  phase: 0.7,  color: 'rgba(255,255,255,0.06)', width: 0.7 },
  ];

  let w, h, t = 0;

  function resize() {
    w = canvas.width  = canvas.offsetWidth;
    h = canvas.height = canvas.offsetHeight;
  }
  window.addEventListener('resize', resize, { passive: true });
  resize();

  function drawLine(line) {
    ctx.beginPath();
    ctx.strokeStyle = line.color;
    ctx.lineWidth   = line.width;
    const y0 = h * 0.5;
    for (let x = 0; x <= w; x += 2) {
      // Modulate amplitude with a slow envelope so the waves breathe
      const envelope = 0.7 + 0.3 * Math.sin(x * 0.003 + t * 0.1);
      const y = y0
        + Math.sin(x * line.freq + t * line.speed + line.phase) * h * line.amp * envelope
        + Math.sin(x * line.freq * 2.3 + t * line.speed * 1.7 + line.phase) * h * line.amp * 0.3 * envelope;
      x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  function frame() {
    ctx.clearRect(0, 0, w, h);
    t += 0.015;
    lines.forEach(drawLine);
    requestAnimationFrame(frame);
  }
  frame();
})();

/* ── Fade-in on scroll ────────────────────────────────────────────── */
const observer = new IntersectionObserver((entries) => {
  entries.forEach(e => {
    if (e.isIntersecting) {
      e.target.style.opacity    = '1';
      e.target.style.transform  = 'translateY(0)';
      observer.unobserve(e.target);
    }
  });
}, { threshold: 0.12 });

document.querySelectorAll('.feature-card, .chain-node, .stat, .param-group').forEach(el => {
  el.style.opacity   = '0';
  el.style.transform = 'translateY(20px)';
  el.style.transition = 'opacity 0.5s ease, transform 0.5s ease';
  observer.observe(el);
});

// ── Listen demo: synced dual-audio toggle (off / on)
(function () {
  const aOff = document.getElementById('audio-off');
  const aOn  = document.getElementById('audio-on');
  const playBtn = document.getElementById('play-btn');
  const pauseBtn = document.getElementById('pause-btn');
  const toggle = document.getElementById('effect-toggle');
  const label = document.getElementById('effect-label');

  if (!aOff || !aOn || !playBtn || !toggle) return;

  // Start both paused; mute the 'on' version by default
  aOff.preload = 'auto';
  aOn.preload  = 'auto';
  aOff.muted = false;
  aOn.muted  = true;

  function setEffectState(isOn) {
    label.textContent = isOn ? 'On' : 'Off';
    aOn.muted  = !isOn;
    aOff.muted = isOn;
    // keep playback positions synced
    try {
      if (!isNaN(aOff.currentTime) && !isNaN(aOn.currentTime)) {
        const t = Math.max(aOff.currentTime, aOn.currentTime);
        aOff.currentTime = t;
        aOn.currentTime  = t;
      }
    } catch (e) { /* ignore */ }
  }

  toggle.addEventListener('change', (e) => setEffectState(e.target.checked));

  playBtn.addEventListener('click', async () => {
    try {
      if (aOff.paused && aOn.paused) {
        aOff.currentTime = 0;
        aOn.currentTime  = 0;
      } else {
        const t = Math.max(aOff.currentTime, aOn.currentTime);
        aOff.currentTime = t;
        aOn.currentTime  = t;
      }
      await Promise.all([aOff.play(), aOn.play()]);
      setEffectState(toggle.checked);
    } catch (err) {
      console.warn('Playback failed:', err);
    }
  });

  pauseBtn.addEventListener('click', () => {
    aOff.pause();
    aOn.pause();
  });

  aOff.addEventListener('ended', () => aOn.pause());
  aOn.addEventListener('ended', () => aOff.pause());
})();
