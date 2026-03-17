/**
 * Card Stack — The Mechanical Researcher
 *
 * Scroll-hijacked card stack interaction:
 * - Cards lift off a desk pile, zoom to fill viewport
 * - Scroll flicks the current card off the top
 * - Next card lifts up in sync
 * - Scrolling back reverses everything
 * - Click flips card and navigates to article
 */

(function () {
  'use strict';

  var cards = window.CARDS || [];
  if (!cards.length) return;

  // Add end card
  cards.push({
    title: '\u2715',
    abstract: '',
    date: '',
    dateDisplay: '',
    category: '',
    url: null,
    isEnd: true
  });

  var activeArea = document.getElementById('active-card-area');
  var stackEl = document.getElementById('card-stack');

  // State
  var currentIndex = 0;       // which card is "active"
  var scrollProgress = 0;     // 0 = card on desk, 1 = card zoomed in, 2 = card flicked off top
  var isAnimating = false;
  var cardElements = [];
  var stackCards = [];         // decorative stack cards

  // --- Create card DOM elements ---

  function createCardEl(data, index) {
    var el = document.createElement('div');
    el.className = 'index-card' + (data.isEnd ? ' end-card' : '');
    el.dataset.index = index;

    if (data.isEnd) {
      el.innerHTML = '<div class="card-title">' + data.title + '</div>';
    } else {
      var permalinkHtml = data.url
        ? '<a class="card-permalink" href="' + escapeHtml(data.url) + '" title="Permalink">&#x1f517;</a>'
        : '';
      el.innerHTML =
        '<div class="card-date">' + escapeHtml(data.dateDisplay) + '</div>' +
        '<div class="card-category">' + escapeHtml(data.category) + '</div>' +
        '<div class="card-title">' + escapeHtml(data.title) + '</div>' +
        '<div class="card-abstract">' + escapeHtml(data.abstract) + '</div>' +
        '<div class="card-footer">' + permalinkHtml + '</div>';
    }

    if (data.url) {
      el.addEventListener('click', function (e) {
        if (e.target.closest('.card-permalink')) return;
        flipAndNavigate(el, data.url);
      });
    }

    return el;
  }

  function escapeHtml(str) {
    var div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }

  // --- Layout calculations ---

  function getViewport() {
    return {
      w: window.innerWidth,
      h: window.innerHeight
    };
  }

  // 5:3 aspect ratio (width:height)
  var CARD_RATIO = 5 / 3;

  // Card size when on the desk (small)
  function deskCardRect() {
    var vp = getViewport();
    var h = Math.min(180, vp.h * 0.25);
    var w = h * CARD_RATIO;
    var stack = stackEl.getBoundingClientRect();
    return {
      width: w,
      height: h,
      x: stack.left + stack.width / 2 - w / 2,
      y: stack.top + stack.height / 2 - h / 2
    };
  }

  // Card size when zoomed to fill viewport
  function zoomedCardRect() {
    var vp = getViewport();
    var pad = 24;
    var barH = 40; // bottom bar
    var maxH = vp.h - pad * 2 - barH;
    var maxW = vp.w - pad * 2;
    // Fit 3:5 card within available space
    var h = maxH;
    var w = h * CARD_RATIO;
    if (w > maxW) {
      w = maxW;
      h = w / CARD_RATIO;
    }
    return {
      width: w,
      height: h,
      x: (vp.w - w) / 2,
      y: pad
    };
  }

  // Card position when flicked off top
  function offTopRect() {
    var zoomed = zoomedCardRect();
    return {
      width: zoomed.width,
      height: zoomed.height,
      x: zoomed.x,
      y: -zoomed.height - 50
    };
  }

  // --- Interpolation ---

  function lerp(a, b, t) {
    return a + (b - a) * t;
  }

  function lerpRect(a, b, t) {
    return {
      width: lerp(a.width, b.width, t),
      height: lerp(a.height, b.height, t),
      x: lerp(a.x, b.x, t),
      y: lerp(a.y, b.y, t)
    };
  }

  // Ease function for smooth animation feel
  function easeInOutCubic(t) {
    return t < 0.5
      ? 4 * t * t * t
      : 1 - Math.pow(-2 * t + 2, 3) / 2;
  }

  // --- Apply position to card element ---
  // oblique: 0 = flat/straight, 1 = full oblique (matching desk perspective)

  // Base font size (px) when card is at zoomed (full) size
  var BASE_FONT = 16;

  function applyRect(el, rect, opacity, oblique) {
    el.style.width = rect.width + 'px';
    el.style.height = rect.height + 'px';
    el.style.left = rect.x + 'px';
    el.style.top = rect.y + 'px';
    el.style.opacity = opacity !== undefined ? opacity : 1;
    // Scale font proportionally to card height vs zoomed height
    var zh = zoomedCardRect().height;
    var scale = zh > 0 ? rect.height / zh : 1;
    el.style.fontSize = (BASE_FONT * scale) + 'px';
    var o = oblique || 0;
    if (o > 0.001) {
      var rotX = 45 * o;
      el.style.transform = 'perspective(50vmin) rotateX(' + rotX + 'deg)';
    } else {
      el.style.transform = 'none';
    }
  }

  // --- Render current state ---

  function render() {
    var desk = deskCardRect();
    var zoomed = zoomedCardRect();
    var offTop = offTopRect();

    for (var i = 0; i < cardElements.length; i++) {
      var el = cardElements[i];
      var cardProgress = (currentIndex * 2 + scrollProgress) - (i * 2);

      if (cardProgress < 0) {
        // Card hasn't been reached yet — sitting on desk stack (oblique)
        applyRect(el, desk, 1, 1);
        el.style.zIndex = cards.length - i;
        el.style.pointerEvents = 'none';
      } else if (cardProgress <= 1) {
        // Phase 1: lifting off desk → zoomed in (oblique → flat)
        var t = easeInOutCubic(Math.max(0, Math.min(1, cardProgress)));
        var rect = lerpRect(desk, zoomed, t);
        applyRect(el, rect, 1, 1 - t);
        el.style.zIndex = 100 + i;
        el.style.pointerEvents = t > 0.8 ? 'auto' : 'none';
      } else if (cardProgress <= 2) {
        // Phase 2: zoomed in → flicked off top (flat)
        var t2 = easeInOutCubic(Math.max(0, Math.min(1, cardProgress - 1)));
        var rect2 = lerpRect(zoomed, offTop, t2);
        applyRect(el, rect2, 1, 0);
        el.style.zIndex = 100 + i;
        el.style.pointerEvents = t2 < 0.2 ? 'auto' : 'none';
      } else {
        // Card is off-screen above
        applyRect(el, offTop, 1, 0);
        el.style.zIndex = i;
        el.style.pointerEvents = 'none';
      }
    }

    // Update decorative stack
    updateStack();
  }

  // --- Decorative stack on desk ---

  function updateStack() {
    // Show stacked cards with 3D depth — stack height ≈ half a card's face height
    var remaining = cards.length - currentIndex - 1;
    var show = Math.min(remaining, 8);
    // Each layer is offset on Z axis; total stack ≈ 60px tall in 3D space
    var layerZ = show > 0 ? 60 / show : 0;

    stackCards.forEach(function (el, i) {
      if (i < show) {
        el.style.display = 'block';
        var z = i * layerZ;
        var jitter = (i % 2 === 0 ? 1 : -1) * (i * 0.4);
        el.style.transform = 'translateZ(' + z + 'px) rotate(' + jitter + 'deg)';
      } else {
        el.style.display = 'none';
      }
    });
  }

  function buildStack() {
    for (var i = 0; i < 8; i++) {
      var el = document.createElement('div');
      el.style.position = 'absolute';
      el.style.width = '100%';
      el.style.height = '100%';
      el.style.background = '#fffef7';
      el.style.border = '1px solid #d4c9b8';
      el.style.borderRadius = '4px';
      el.style.boxShadow = '0 1px 2px rgba(0,0,0,0.12)';
      el.style.transformStyle = 'preserve-3d';
      stackEl.appendChild(el);
      stackCards.push(el);
    }
  }

  // --- Scroll handling ---

  var scrollAccum = 0;
  var SCROLL_THRESHOLD = 600; // pixels of scroll per phase transition (higher = slower)

  function onWheel(e) {
    e.preventDefault();
    if (isAnimating) return;

    scrollAccum += e.deltaY;

    // Convert accumulated scroll into progress
    var delta = scrollAccum / SCROLL_THRESHOLD;
    scrollAccum = 0;

    var totalProgress = currentIndex * 2 + scrollProgress + delta;
    totalProgress = Math.max(0, Math.min(totalProgress, (cards.length - 1) * 2 + 1));

    currentIndex = Math.floor(totalProgress / 2);
    scrollProgress = totalProgress - currentIndex * 2;

    render();
  }

  // Touch support
  var touchStartY = 0;
  var touchLastY = 0;

  function onTouchStart(e) {
    touchStartY = e.touches[0].clientY;
    touchLastY = touchStartY;
  }

  function onTouchMove(e) {
    e.preventDefault();
    if (isAnimating) return;

    var y = e.touches[0].clientY;
    var delta = (touchLastY - y) / SCROLL_THRESHOLD;
    touchLastY = y;

    var totalProgress = currentIndex * 2 + scrollProgress + delta;
    totalProgress = Math.max(0, Math.min(totalProgress, (cards.length - 1) * 2 + 1));

    currentIndex = Math.floor(totalProgress / 2);
    scrollProgress = totalProgress - currentIndex * 2;

    render();
  }

  // --- Card flip & navigate ---

  function flipAndNavigate(el, url) {
    isAnimating = true;
    el.style.transition = 'transform 0.5s ease-in-out, opacity 0.3s ease 0.3s';
    el.style.transformOrigin = 'center center';
    el.style.transform = 'rotateY(-180deg) scale(1.5)';

    setTimeout(function () {
      // Fill screen with card color
      document.body.style.transition = 'background 0.2s';
      document.body.style.background = '#fffef7';
    }, 300);

    setTimeout(function () {
      window.location.href = url;
    }, 600);
  }

  // --- Entry animation ---

  function animateEntry() {
    // Start with card on desk, then lift it
    scrollProgress = 0;
    render();

    // Animate lift over 800ms
    var start = null;
    var duration = 800;

    function step(ts) {
      if (!start) start = ts;
      var elapsed = ts - start;
      var t = Math.min(elapsed / duration, 1);

      scrollProgress = easeInOutCubic(t);
      render();

      if (t < 1) {
        requestAnimationFrame(step);
      } else {
        isAnimating = false;
      }
    }

    isAnimating = true;
    requestAnimationFrame(step);
  }

  // --- Grid (exploded) view ---

  var gridMode = false;
  var gridOverlay = document.getElementById('grid-overlay');
  var gridContainer = document.getElementById('grid-container');
  var gridCloseBtn = document.getElementById('grid-close');

  function buildGridCards() {
    gridContainer.innerHTML = '';
    cards.forEach(function (data) {
      if (data.isEnd) return;
      var el = document.createElement('div');
      el.className = 'grid-card';
      var permalinkHtml = data.url
        ? '<a class="card-permalink" href="' + escapeHtml(data.url) + '" title="Permalink">&#x1f517;</a>'
        : '';
      el.innerHTML =
        '<div class="card-date">' + escapeHtml(data.dateDisplay) + '</div>' +
        '<div class="card-category">' + escapeHtml(data.category) + '</div>' +
        '<div class="card-title">' + escapeHtml(data.title) + '</div>' +
        '<div class="card-abstract">' + escapeHtml(data.abstract) + '</div>' +
        '<div class="card-footer">' + permalinkHtml + '</div>';
      if (data.url) {
        el.addEventListener('click', function (e) {
          if (e.target.closest('.card-permalink')) return;
          window.location.href = data.url;
        });
      }
      gridContainer.appendChild(el);
    });
  }

  function enterGridMode() {
    if (gridMode) return;
    gridMode = true;
    buildGridCards();
    gridOverlay.classList.add('active');

    // Disable stack scroll
    window.removeEventListener('wheel', onWheel);
    window.removeEventListener('touchstart', onTouchStart);
    window.removeEventListener('touchmove', onTouchMove);

    // Hide stack cards
    cardElements.forEach(function (el) {
      el.style.display = 'none';
    });
  }

  function exitGridMode() {
    if (!gridMode) return;
    gridMode = false;
    gridOverlay.classList.remove('active');

    // Restore stack scroll
    window.addEventListener('wheel', onWheel, { passive: false });
    window.addEventListener('touchstart', onTouchStart, { passive: true });
    window.addEventListener('touchmove', onTouchMove, { passive: false });

    // Show stack cards again
    cardElements.forEach(function (el) {
      el.style.display = '';
    });
    render();
  }

  // --- Init ---

  function init() {
    // Build card elements
    cards.forEach(function (data, i) {
      var el = createCardEl(data, i);
      activeArea.appendChild(el);
      cardElements.push(el);
    });

    buildStack();

    // Set up scroll
    window.addEventListener('wheel', onWheel, { passive: false });
    window.addEventListener('touchstart', onTouchStart, { passive: true });
    window.addEventListener('touchmove', onTouchMove, { passive: false });

    // Mug click → explode to grid
    var mugEl = document.querySelector('.mug');
    if (mugEl) {
      mugEl.addEventListener('click', enterGridMode);
    }

    // Close button → back to stack
    if (gridCloseBtn) {
      gridCloseBtn.addEventListener('click', exitGridMode);
    }

    // Resize handler
    window.addEventListener('resize', function () {
      if (!gridMode) render();
    });

    // Entry animation
    animateEntry();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
