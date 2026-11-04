(() => {
  const $ = (id) => document.getElementById(id);
  const KEY_FIELDS = [
    ["power_up", "P+", "keyboard_arrow_up"],
    ["power_down", "P−", "keyboard_arrow_down"],
    ["power_off", "P0", "power_settings_new"],
    ["brake_up", "B+", "add"],
    ["brake_down", "B−", "remove"],
    ["brake_eb", "EB", "warning"],
    ["pedal", "Pedal", "volume_up"],
    ["btn_a", "A", "looks_one"],
    ["btn_b", "B", "looks_two"],
    ["btn_c", "C", "looks_3"],
    ["btn_d", "D", "looks_4"],
    ["btn_select", "SEL", "radio_button_checked"],
    ["btn_start", "ST", "play_circle"],
    ["hat_u", "↑", "arrow_upward"],
    ["hat_d", "↓", "arrow_downward"],
    ["hat_l", "←", "arrow_back"],
    ["hat_r", "→", "arrow_forward"],
  ];
  // keymap uses 4 columns in CSS; 17 fields fills left panel without large void

  const TOAST_ICONS = {
    toast_saved: "save",
    toast_loaded: "folder_open",
    toast_applied: "done",
    toast_test_door: "lightbulb",
    toast_test_vib: "vibration",
    toast_test_off: "power_settings_new",
    toast_started: "play_arrow",
    toast_stopped: "stop",
    toast_theme: "dark_mode",
    toast_driver_ok: "usb",
    toast_driver_bad: "report_problem",
    toast_driver_busy: "hourglass_top",
    toast_driver_missing: "usb_off",
    toast_driver_wrong: "report_problem",
  };

  let lang = localStorage.getItem("mstn_lang") || "th";
  let theme = localStorage.getItem("mstn_theme") || "dark";
  let toastTimer = 0;
  let lastKeys = {};
  let mapField = null;
  let mapListen = false;

  const FIELD_LABEL = Object.fromEntries(KEY_FIELDS.map(([id, label]) => [id, label]));
  const AXIS_GROUPS = {
    power: [["power_up", "P+"], ["power_down", "P−"], ["power_off", "P0"]],
    brake: [["brake_up", "B+"], ["brake_down", "B−"], ["brake_eb", "EB"]],
  };
  const KEY_CATALOG = (() => {
    const letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("");
    const digits = "0123456789".split("");
    const special = [
      ["", "— clear —"],
      ["SPACE", "Space"],
      ["RETURN", "Enter"],
      ["UP", "↑ Up"],
      ["DOWN", "↓ Down"],
      ["LEFT", "← Left"],
      ["RIGHT", "→ Right"],
      ["OEM_PERIOD", ". Period"],
      ["OEM_COMMA", ", Comma"],
      ["OEM_2", "/ Slash"],
      ["ADD", "+ Add"],
      ["INSERT", "Insert"],
      ["DELETE", "Delete"],
      ["HOME", "Home"],
      ["END", "End"],
      ["PRIOR", "PageUp"],
      ["NEXT", "PageDown"],
      ["ESCAPE", "Esc"],
      ["TAB", "Tab"],
      ["OEM_1", ";"],
      ["OEM_4", "["],
      ["OEM_6", "]"],
      ["OEM_5", "\\"],
      ["OEM_7", "'"],
      ["OEM_MINUS", "-"],
      ["OEM_PLUS", "="],
    ];
    return [
      ...special,
      ...letters.map((c) => [c, c]),
      ...digits.map((c) => [c, c]),
    ];
  })();
  let mapDraft = "";

  function keyEventToName(e) {
    const code = e.code || "";
    const key = e.key || "";
    if (key === " " || code === "Space") return "SPACE";
    if (key === "Enter" || code === "Enter" || code === "NumpadEnter") return "RETURN";
    if (key === "ArrowUp" || code === "ArrowUp") return "UP";
    if (key === "ArrowDown" || code === "ArrowDown") return "DOWN";
    if (key === "ArrowLeft" || code === "ArrowLeft") return "LEFT";
    if (key === "ArrowRight" || code === "ArrowRight") return "RIGHT";
    if (key === "Insert" || code === "Insert") return "INSERT";
    if (key === "Delete" || code === "Delete") return "DELETE";
    if (key === "Home" || code === "Home") return "HOME";
    if (key === "End" || code === "End") return "END";
    if (key === "." || code === "Period" || code === "NumpadDecimal") return "OEM_PERIOD";
    if (key === "," || code === "Comma") return "OEM_COMMA";
    if (key === "/" || code === "Slash") return "OEM_2";
    if (key === "+" || code === "NumpadAdd") return "ADD";
    if (/^[a-zA-Z]$/.test(key)) return key.toUpperCase();
    if (/^[0-9]$/.test(key)) return key;
    if (code.startsWith("Key") && code.length === 4) return code.slice(3);
    if (code.startsWith("Digit") && code.length === 6) return code.slice(5);
    return "";
  }

  function applyKeyMap(field, value) {
    const label = FIELD_LABEL[field] || field;
    const from = lastKeys[field] || "";
    lastKeys[field] = value;
    post({ type: "cmd", cmd: "set_key", field, value });
    toast("toast_applied", `${label}: ${from || "∅"} → ${value || "∅"}`);
    refreshMonitorKeyTips();
  }

  function closeMapPop() {
    mapField = null;
    mapListen = false;
    mapDraft = "";
    const pop = $("mapPop");
    if (pop) {
      pop.hidden = true;
      pop.classList.remove("group-mode");
    }
    document.querySelectorAll(".map-focus").forEach((el) => el.classList.remove("map-focus"));
    const keyEl = $("mapPopKey");
    if (keyEl) keyEl.classList.remove("listen");
  }

  function fillMapSelect(selected) {
    const sel = $("mapPopSelect");
    if (!sel) return;
    const want = selected || "";
    let html = "";
    for (const [value, label] of KEY_CATALOG) {
      const on = value === want ? " selected" : "";
      html += `<option value="${value}"${on}>${label}</option>`;
    }
    if (want && !KEY_CATALOG.some(([v]) => v === want)) {
      html = `<option value="${want}" selected>${want}</option>` + html;
    }
    sel.innerHTML = html;
    sel.value = want;
    // re-sync custom glass select after options rebuild
    if (sel.dataset.enhanced === "1") syncCustomSelect(sel);
  }

  function setMapDraft(value) {
    mapDraft = value || "";
    const keyEl = $("mapPopKey");
    if (keyEl) keyEl.textContent = shortKey(mapDraft) || "∅";
    fillMapSelect(mapDraft);
  }

  function positionMapPop(anchor) {
    const pop = $("mapPop");
    pop.style.transform = "";
    const r = anchor.getBoundingClientRect();
    pop.hidden = false;
    const pw = pop.offsetWidth || 220;
    const ph = pop.offsetHeight || 120;
    let left = r.left + r.width / 2 - pw / 2;
    let top = r.bottom + 8;
    left = Math.max(8, Math.min(left, window.innerWidth - pw - 8));
    if (top + ph > window.innerHeight - 8) top = Math.max(8, r.top - ph - 8);
    pop.style.left = left + "px";
    pop.style.top = top + "px";
  }

  function openMapField(field, anchor) {
    closeMapPop();
    mapField = field;
    mapListen = true;
    const pop = $("mapPop");
    pop.classList.remove("group-mode");
    const list = $("mapPopList");
    list.hidden = true;
    list.innerHTML = "";
    $("mapPopLabel").textContent = FIELD_LABEL[field] || field;
    const cur = lastKeys[field] || "";
    const keyEl = $("mapPopKey");
    keyEl.classList.add("listen");
    setMapDraft(cur);
    $("mapPopHint").textContent = t("map_hint");
    if (anchor) {
      anchor.classList.add("map-focus");
      positionMapPop(anchor);
    } else {
      pop.hidden = false;
      pop.style.left = "50%";
      pop.style.top = "40%";
      pop.style.transform = "translate(-50%, -50%)";
    }
  }

  function openAxisGroup(kind, anchor) {
    closeMapPop();
    const rows = AXIS_GROUPS[kind];
    if (!rows) return;
    mapListen = false;
    mapField = null;
    const pop = $("mapPop");
    pop.classList.add("group-mode");
    const list = $("mapPopList");
    list.hidden = false;
    list.innerHTML = "";
    $("mapPopLabel").textContent = kind === "power" ? t("power") : t("brake");
    $("mapPopHint").textContent = t("map_group_hint");
    for (const [id, label] of rows) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.innerHTML = `<span>${label}</span><code>${shortKey(lastKeys[id] || "")}</code>`;
      btn.onclick = (e) => {
        e.stopPropagation();
        openMapField(id, anchor);
      };
      list.appendChild(btn);
    }
    if (anchor) {
      anchor.classList.add("map-focus");
      positionMapPop(anchor);
    } else {
      pop.hidden = false;
    }
  }

  function shortKey(name) {
    if (!name) return "∅";
    if (name === "OEM_PERIOD") return ".";
    if (name === "OEM_COMMA") return ",";
    if (name === "OEM_2") return "/";
    if (name === "RETURN") return "↵";
    if (name === "SPACE") return "␣";
    if (name === "UP") return "↑";
    if (name === "DOWN") return "↓";
    if (name === "LEFT") return "←";
    if (name === "RIGHT") return "→";
    return name;
  }

  function refreshMonitorKeyTips() {
    document.querySelectorAll(".pad-cell[data-k]").forEach((el) => {
      const k = el.getAttribute("data-k");
      if (!k) return;
      const label = FIELD_LABEL[k] || k;
      const key = lastKeys[k] || "";
      el.title = `${label} → ${key || "∅"}`;
      el.classList.add("map-hit");
      el.classList.toggle("empty-map", !key);
      const chip = el.querySelector(":scope > .key-chip");
      if (chip) chip.textContent = shortKey(key);
    });

    function fillAxis(id, rows) {
      const root = $(id);
      if (!root) return;
      root.innerHTML = "";
      for (const [field, label] of rows) {
        const row = document.createElement("button");
        row.type = "button";
        row.className = "axis-key map-hit";
        row.dataset.mapField = field;
        const key = lastKeys[field] || "";
        row.innerHTML = `<span class="axis-tag">${label}</span><span class="keycap">${shortKey(key)}</span>`;
        row.title = `${label} → ${key || "∅"}`;
        row.onclick = (e) => {
          e.stopPropagation();
          openMapField(field, row);
        };
        root.appendChild(row);
      }
    }
    fillAxis("powerKeys", AXIS_GROUPS.power);
    fillAxis("brakeKeys", AXIS_GROUPS.brake);

    const ps = $("powerStack");
    const bs = $("brakeStack");
    if (ps) {
      ps.classList.add("map-hit");
      ps.title = `P+ ${shortKey(lastKeys.power_up)} · P− ${shortKey(lastKeys.power_down)} · P0 ${shortKey(lastKeys.power_off)}`;
    }
    if (bs) {
      bs.classList.add("map-hit");
      bs.title = `B+ ${shortKey(lastKeys.brake_up)} · B− ${shortKey(lastKeys.brake_down)} · EB ${shortKey(lastKeys.brake_eb)}`;
    }
  }

  function t(key) {
    const pack = (window.MSTN_I18N && window.MSTN_I18N[lang]) || {};
    return pack[key] || (window.MSTN_I18N.en[key] || key);
  }

  function applyI18n() {
    document.documentElement.lang = lang === "ja" ? "ja" : lang === "en" ? "en" : "th";
    document.querySelectorAll("[data-i18n]").forEach((el) => {
      const k = el.getAttribute("data-i18n");
      el.textContent = t(k);
    });
    document.querySelectorAll("[data-i18n-title]").forEach((el) => {
      const k = el.getAttribute("data-i18n-title");
      el.title = t(k);
      el.setAttribute("aria-label", t(k));
    });
    document.querySelectorAll("select").forEach(syncCustomSelect);
  }

  function setTheme(next) {
    theme = next;
    document.documentElement.setAttribute("data-theme", theme);
    localStorage.setItem("mstn_theme", theme);
    const ic = $("themeIcon");
    if (ic) ic.textContent = theme === "dark" ? "dark_mode" : "light_mode";
  }

  function toast(titleKey, body) {
    const el = $("toast");
    const key = typeof titleKey === "string" ? titleKey : "info";
    const title = t(key) !== key ? t(key) : titleKey;
    $("toastTitle").textContent = title;
    $("toastBody").textContent = body || "";
    const ic = $("toastIcon");
    if (ic) ic.textContent = TOAST_ICONS[key] || "info";
    el.hidden = false;
    requestAnimationFrame(() => el.classList.add("show"));
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
      el.classList.remove("show");
      setTimeout(() => { el.hidden = true; }, 200);
    }, 2200);
  }

  function post(obj) {
    const s = typeof obj === "string" ? obj : JSON.stringify(obj);
    if (window.chrome && window.chrome.webview) window.chrome.webview.postMessage(s);
  }

  function menuForSelect(sel) {
    if (!sel) return null;
    return document.querySelector(`.cselect-menu[data-owner="${sel.id}"]`);
  }

  function closeAllSelects(except) {
    document.querySelectorAll(".cselect.open").forEach((wrap) => {
      if (wrap === except) return;
      wrap.classList.remove("open");
    });
    document.querySelectorAll(".cselect-menu").forEach((menu) => {
      if (except && menu.dataset.owner && except.querySelector(`#${CSS.escape(menu.dataset.owner)}`))
        return;
      menu.hidden = true;
      menu.classList.remove("cselect-portal");
      menu.style.left = "";
      menu.style.top = "";
      menu.style.width = "";
      const ownerId = menu.dataset.owner;
      const wrap = ownerId && $(ownerId) ? $(ownerId).closest(".cselect") : null;
      if (wrap && menu.parentElement !== wrap) wrap.appendChild(menu);
    });
  }

  function placeSelectMenu(menu, btn) {
    document.body.appendChild(menu);
    menu.classList.add("cselect-portal");
    const r = btn.getBoundingClientRect();
    const width = Math.max(r.width, 128);
    let left = r.left;
    let top = r.bottom + 4;
    menu.hidden = false;
    const mh = Math.min(menu.scrollHeight || 180, 220);
    if (top + mh > window.innerHeight - 8) top = Math.max(8, r.top - mh - 4);
    if (left + width > window.innerWidth - 8) left = Math.max(8, window.innerWidth - width - 8);
    menu.style.left = left + "px";
    menu.style.top = top + "px";
    menu.style.width = width + "px";
  }

  function syncCustomSelect(sel) {
    const wrap = sel.closest(".cselect");
    if (!wrap) return;
    const label = wrap.querySelector(".cselect-label");
    const opt = sel.options[sel.selectedIndex];
    if (label) label.textContent = opt ? opt.textContent : "";
    const menu = menuForSelect(sel) || wrap.querySelector(".cselect-menu");
    if (!menu) return;
    menu.querySelectorAll(".cselect-opt").forEach((btn) => {
      btn.classList.toggle("on", btn.dataset.value === sel.value);
    });
  }

  function enhanceSelect(sel) {
    if (!sel || sel.dataset.enhanced === "1") return;
    sel.dataset.enhanced = "1";
    if (!sel.id) sel.id = "sel_" + Math.random().toString(36).slice(2, 9);
    sel.classList.add("cselect-native");

    const wrap = document.createElement("div");
    wrap.className = "cselect";
    sel.parentNode.insertBefore(wrap, sel);
    wrap.appendChild(sel);

    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "cselect-btn";
    btn.innerHTML = `<span class="cselect-label"></span><span class="material-icons">expand_more</span>`;
    wrap.appendChild(btn);

    const menu = document.createElement("div");
    menu.className = "cselect-menu glass";
    menu.dataset.owner = sel.id;
    menu.hidden = true;
    menu.addEventListener("click", (e) => e.stopPropagation());
    wrap.appendChild(menu);

    function rebuild() {
      menu.innerHTML = "";
      Array.from(sel.options).forEach((opt) => {
        const item = document.createElement("button");
        item.type = "button";
        item.className = "cselect-opt" + (opt.value === sel.value ? " on" : "");
        item.dataset.value = opt.value;
        item.textContent = opt.textContent;
        item.onclick = (e) => {
          e.stopPropagation();
          sel.value = opt.value;
          sel.dispatchEvent(new Event("change", { bubbles: true }));
          syncCustomSelect(sel);
          closeAllSelects();
        };
        menu.appendChild(item);
      });
      syncCustomSelect(sel);
    }

    btn.onclick = (e) => {
      e.stopPropagation();
      const willOpen = menu.hidden;
      closeAllSelects();
      if (willOpen) {
        rebuild();
        wrap.classList.add("open");
        placeSelectMenu(menu, btn);
      }
    };

    sel.addEventListener("change", () => syncCustomSelect(sel));
    const mo = new MutationObserver(() => syncCustomSelect(sel));
    mo.observe(sel, { childList: true, subtree: true, characterData: true });
    rebuild();
  }

  function enhanceAllSelects() {
    document.querySelectorAll("select").forEach(enhanceSelect);
  }

  function setSelectValue(id, value) {
    const sel = $(id);
    if (!sel || value == null) return;
    if (sel.value === value) {
      syncCustomSelect(sel);
      return;
    }
    sel.value = value;
    syncCustomSelect(sel);
  }

  function buildBits() {
    const root = $("bitLamps");
    root.innerHTML = "";
    for (let i = 0; i < 16; i++) {
      const d = document.createElement("div");
      d.className = "bit";
      d.dataset.bit = String(i);
      d.textContent = String(i + 1);
      root.appendChild(d);
    }
  }

  function powerLabel(n) { return "P" + n; }
  function brakeLabel(n, max) {
    if (n <= 0) return "N";
    if (n >= max) return "EB";
    return "B" + n;
  }

  let stackSig = "";
  function renderStacks(power, brake, pmax, bmax) {
    const ps = $("powerStack");
    const bs = $("brakeStack");
    const sig = `${pmax}:${bmax}`;
    if (stackSig !== sig) {
      stackSig = sig;
      ps.innerHTML = "";
      bs.innerHTML = "";
      for (let i = 0; i <= pmax; i++) {
        const el = document.createElement("div");
        el.dataset.n = String(i);
        el.textContent = powerLabel(i);
        ps.appendChild(el);
      }
      for (let i = 0; i <= bmax; i++) {
        const el = document.createElement("div");
        el.dataset.n = String(i);
        el.textContent = brakeLabel(i, bmax);
        bs.appendChild(el);
      }
    }
    ps.querySelectorAll(".step, [data-n]").forEach((el) => {
      const i = Number(el.dataset.n);
      const lit = i <= power;
      el.className = "step n" + i + (lit ? " lit" : "") + (i === power ? " on" : "");
    });
    bs.querySelectorAll(".step, [data-n]").forEach((el) => {
      const i = Number(el.dataset.n);
      const lit = i <= brake;
      el.className = "step n" + i + (lit ? " lit" : "") + (i === brake ? " on" : "");
    });
  }

  function setOn(sel, on) {
    document.querySelectorAll(sel).forEach((el) => el.classList.toggle("on", !!on));
  }

  function applyState(st) {
    if (st.toast) {
      toast(st.toast.title || "OK", st.toast.body || "");
    }
    if (st.driver_prompt) {
      $("driverModal").hidden = false;
    }

    const connected = !!st.connected;
    const pill = $("conn");
    if (st.driver === "needs_winusb") {
      pill.textContent = "NO DRIVER";
      pill.className = "pill warn";
    } else {
      pill.textContent = connected ? "CONNECTED" : "IDLE";
      pill.className = "pill " + (connected ? "on" : "off");
    }

    const statusMap = {
      idle: lang === "th" ? "ว่าง" : lang === "ja" ? "待機" : "Idle",
      searching: lang === "th" ? "กำลังค้นหา…" : lang === "ja" ? "検索中…" : "Searching…",
      ready: lang === "th" ? "พร้อมรับข้อมูล" : lang === "ja" ? "準備完了" : "Ready",
      live: lang === "th" ? "กำลังอ่านจอย" : lang === "ja" ? "受信中" : "Live",
      missing: lang === "th" ? "ไม่พบจอย" : lang === "ja" ? "未検出" : "Not found",
      busy: lang === "th" ? "ถูกใช้งานอยู่" : lang === "ja" ? "使用中" : "Busy",
      needs_winusb: lang === "th" ? "ต้องติดตั้งไดรเวอร์" : lang === "ja" ? "要ドライバ" : "Need WinUSB",
    };
    const rawStatus = st.status || "idle";
    const statusEl = $("statusText");
    statusEl.textContent = statusMap[rawStatus] || rawStatus;
    statusEl.className = "status-badge" + (
      rawStatus === "live" || rawStatus === "ready" ? " ok" :
      rawStatus === "needs_winusb" || rawStatus === "busy" || rawStatus === "searching" ? " warn" :
      rawStatus === "missing" ? " bad" : ""
    );

    $("hz").textContent = (st.hz || 0).toFixed(1);
    $("hex").textContent = st.hex && st.hex.length ? st.hex : "—";
    const hx = (v) => (typeof v === "number" && v >= 0 ? v.toString(16).toUpperCase().padStart(2, "0") : "—");
    $("br").textContent = hx(st.br);
    $("ms").textContent = hx(st.ms);
    $("pd").textContent = hx(st.pd);
    $("ht").textContent = hx(st.ht);
    $("bt").textContent = hx(st.bt);
    $("powerRaw").textContent = typeof st.ms === "number" && st.ms >= 0 ? "MS " + hx(st.ms) : "MS —";
    $("brakeRaw").textContent = typeof st.br === "number" && st.br >= 0 ? "BR " + hx(st.br) : "BR —";
    const decParts = [st.br, st.ms, st.pd, st.ht, st.bt]
      .map((v) => (typeof v === "number" && v >= 0 ? String(v) : null))
      .filter(Boolean);
    $("dec").textContent = decParts.length ? decParts.join(" · ") : "—";

    const pmax = st.power_max ?? 5;
    const bmax = st.brake_max ?? 9;
    $("powerVal").textContent = powerLabel(st.power || 0);
    $("brakeVal").textContent = brakeLabel(st.brake || 0, bmax);
    renderStacks(st.power || 0, st.brake || 0, pmax, bmax);

    if (Array.isArray(st.bits)) {
      document.querySelectorAll("#bitLamps .bit").forEach((el) => {
        const i = Number(el.dataset.bit);
        el.classList.toggle("on", !!st.bits[i]);
      });
    }

    for (const k of ["btn_a", "btn_b", "btn_c", "btn_d", "btn_select", "btn_start", "hat_u", "hat_d", "hat_l", "hat_r", "pedal"]) {
      setOn(`.pad-cell[data-k="${k}"]`, st[k]);
    }

    $("door").classList.toggle("on", !!st.door);
    $("vibl").classList.toggle("on", !!st.vib_l);
    $("vibr").classList.toggle("on", !!st.vib_r);
    $("atc").textContent = st.atc ?? 0;
    $("speed").textContent = st.speed ?? 0;

    if (typeof st.output === "boolean") $("outEnable").checked = st.output;
    if (typeof st.disable_feedback === "boolean") $("disableFb").checked = st.disable_feedback;
    if (st.push) setSelectValue("pushMode", st.push);
    if (typeof st.hold === "number") $("holdMs").value = st.hold;
    if (st.device_kind) setSelectValue("deviceKind", st.device_kind);
    if (st.game_preset) setSelectValue("gamePreset", st.game_preset);

    if (st.keys) {
      for (const [id] of KEY_FIELDS) {
        if (typeof st.keys[id] === "string") lastKeys[id] = st.keys[id];
      }
      refreshMonitorKeyTips();
      if (mapField && mapListen) setMapDraft(lastKeys[mapField] || "");
    }
  }

  // init
  setTheme(theme);
  $("lang").value = lang;
  enhanceAllSelects();
  buildBits();
  renderStacks(0, 0, 5, 9);
  applyI18n();
  setSelectValue("lang", lang);
  refreshMonitorKeyTips();

  document.addEventListener("click", (e) => {
    closeAllSelects();
    const pop = $("mapPop");
    if (pop && !pop.hidden && !pop.contains(e.target) && !e.target.closest(".map-hit")) {
      closeMapPop();
    }
  });

  // Monitor click → show / remap binding
  document.addEventListener("click", (e) => {
    const hit = e.target.closest(".pad-cell[data-k], [data-k]");
    if (hit && hit.closest(".mon, .pad-card, .notch-card")) {
      const k = hit.getAttribute("data-k");
      if (!k || k === "hat_c") return;
      if (!FIELD_LABEL[k]) return;
      e.stopPropagation();
      openMapField(k, hit);
      return;
    }
    const stack = e.target.closest("#powerStack, #brakeStack");
    if (stack) {
      e.stopPropagation();
      openAxisGroup(stack.id === "powerStack" ? "power" : "brake", stack);
    }
  }, true);

  $("mapPopClose").onclick = (e) => {
    e.stopPropagation();
    closeMapPop();
  };
  $("mapPop").addEventListener("click", (e) => e.stopPropagation());
  $("mapPopSelect").onchange = () => {
    if (!mapListen || !mapField) return;
    setMapDraft($("mapPopSelect").value || "");
  };
  $("mapPopClear").onclick = (e) => {
    e.stopPropagation();
    if (!mapField) return;
    applyKeyMap(mapField, "");
    setMapDraft("");
  };
  $("mapPopApply").onclick = (e) => {
    e.stopPropagation();
    if (!mapField) return;
    applyKeyMap(mapField, mapDraft || "");
    closeMapPop();
  };

  document.addEventListener("keydown", (e) => {
    if (!mapListen || !mapField) return;
    const tag = (e.target && e.target.tagName) || "";
    if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return;
    // ignore while using custom select menu
    if (e.target && e.target.closest && e.target.closest(".cselect-menu, .cselect-btn")) return;
    e.preventDefault();
    e.stopPropagation();
    if (e.key === "Escape") {
      closeMapPop();
      return;
    }
    if (e.key === "Enter") {
      applyKeyMap(mapField, mapDraft || "");
      closeMapPop();
      return;
    }
    if (e.key === "Backspace" || e.key === "Delete") {
      setMapDraft("");
      return;
    }
    const name = keyEventToName(e);
    if (!name) return;
    setMapDraft(name);
  }, true);

  $("lang").onchange = () => {
    lang = $("lang").value;
    localStorage.setItem("mstn_lang", lang);
    applyI18n();
    toast("toast_applied", lang.toUpperCase());
  };
  $("btnTheme").onclick = () => {
    setTheme(theme === "dark" ? "light" : "dark");
    toast("toast_theme", theme);
  };
  $("btnLoad").onclick = () => post({ type: "cmd", cmd: "load" });
  $("btnSave").onclick = () => {
    post({
      type: "cmd", cmd: "save_preset",
      game: $("gamePreset").value,
      device: $("deviceKind").value,
    });
  };
  $("btnApply").onclick = () => {
    post({
      type: "cmd", cmd: "set_output",
      enabled: $("outEnable").checked,
      push: $("pushMode").value,
      hold: Number($("holdMs").value),
      disable_feedback: $("disableFb").checked,
      device_kind: $("deviceKind").value,
      game_preset: $("gamePreset").value,
    });
  };
  $("btnStart").onclick = () => post({ type: "cmd", cmd: "start_usb" });
  $("btnStop").onclick = () => post({ type: "cmd", cmd: "stop_usb" });
  $("btnTestDoor").onclick = () => post({ type: "cmd", cmd: "test_door" });

  // Vib L / R / Both — hold to rumble that side; combine freely
  const vibHold = { l: false, r: false, both: false };
  function pushVibState() {
    const left = vibHold.l || vibHold.both;
    const right = vibHold.r || vibHold.both;
    post({ type: "cmd", cmd: "test_vib_set", left, right });
  }
  function bindVibHold(btn, key) {
    if (!btn) return;
    let held = false;
    const down = (e) => {
      if (e && e.button != null && e.button !== 0) return;
      e && e.preventDefault();
      if (held) return;
      held = true;
      vibHold[key] = true;
      try { btn.setPointerCapture(e.pointerId); } catch (_) {}
      btn.classList.add("armed");
      pushVibState();
    };
    const up = (e) => {
      if (!held) return;
      held = false;
      vibHold[key] = false;
      btn.classList.remove("armed");
      if (e && e.pointerId != null) {
        try { btn.releasePointerCapture(e.pointerId); } catch (_) {}
      }
      pushVibState();
    };
    btn.addEventListener("pointerdown", down);
    btn.addEventListener("pointerup", up);
    btn.addEventListener("pointercancel", up);
    btn.addEventListener("lostpointercapture", () => {
      if (!held) return;
      held = false;
      vibHold[key] = false;
      btn.classList.remove("armed");
      pushVibState();
    });
    btn.addEventListener("contextmenu", (e) => e.preventDefault());
  }
  bindVibHold($("btnTestVibL"), "l");
  bindVibHold($("btnTestVibR"), "r");
  bindVibHold($("btnTestVibBoth"), "both");

  $("btnTestOff").onclick = () => post({ type: "cmd", cmd: "test_off" });
  $("btnProbeDriver").onclick = () => post({ type: "cmd", cmd: "probe_driver" });
  $("btnDriverClose").onclick = () => { $("driverModal").hidden = true; };
  $("btnOpenZadig").onclick = () => post({ type: "cmd", cmd: "open_zadig" });

  // block drag-select chrome
  document.addEventListener("selectstart", (e) => {
    const tag = (e.target && e.target.tagName) || "";
    if (tag !== "INPUT" && tag !== "TEXTAREA" && tag !== "SELECT") e.preventDefault();
  });
  document.addEventListener("dragstart", (e) => e.preventDefault());

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener("message", (ev) => {
      let data = ev.data;
      if (typeof data === "string") {
        try { data = JSON.parse(data); } catch { return; }
      }
      if (data && data.type === "state") applyState(data);
      if (data && data.type === "toast") toast(data.title || "OK", data.body || "");
    });
  }
})();
