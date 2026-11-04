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
  };

  let lang = localStorage.getItem("mstn_lang") || "th";
  let theme = localStorage.getItem("mstn_theme") || "dark";
  let toastTimer = 0;
  let lastKeys = {};

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

  function buildKeys() {
    const root = $("keyFields");
    root.innerHTML = "";
    for (const [id, label, icon] of KEY_FIELDS) {
      const lab = document.createElement("label");
      lab.innerHTML = `<span class="klab"><span class="material-icons md-14">${icon}</span> ${label}</span>`;
      const inp = document.createElement("input");
      inp.type = "text";
      inp.id = "key_" + id;
      inp.dataset.field = id;
      inp.addEventListener("change", () => {
        const from = lastKeys[id] || "";
        const to = inp.value;
        post({ type: "cmd", cmd: "set_key", field: id, value: to });
        toast("toast_applied", `${label}: ${from || "∅"} → ${to || "∅"}`);
        lastKeys[id] = to;
      });
      lab.appendChild(inp);
      root.appendChild(lab);
    }
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

  function renderStacks(power, brake, pmax, bmax) {
    const ps = $("powerStack");
    const bs = $("brakeStack");
    ps.innerHTML = "";
    bs.innerHTML = "";
    for (let i = 0; i <= pmax; i++) {
      const el = document.createElement("div");
      el.className = "step" + (i === power ? " on" : "");
      el.textContent = powerLabel(i);
      ps.appendChild(el);
    }
    for (let i = 0; i <= bmax; i++) {
      const el = document.createElement("div");
      el.className = "step" + (i === brake ? " on" : "");
      el.textContent = brakeLabel(i, bmax);
      bs.appendChild(el);
    }
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

    $("statusText").textContent = st.status || "";
    $("hz").textContent = (st.hz || 0).toFixed(1);
    $("hex").textContent = st.hex || "--";
    const hx = (v) => (v >= 0 ? v.toString(16).toUpperCase().padStart(2, "0") : "--");
    $("br").textContent = hx(st.br);
    $("ms").textContent = hx(st.ms);
    $("pd").textContent = hx(st.pd);
    $("ht").textContent = hx(st.ht);
    $("bt").textContent = hx(st.bt);
    $("dec").textContent = [st.br, st.ms, st.pd, st.ht, st.bt].map((v) => (v >= 0 ? v : "-")).join(" ");

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
      setOn(`[data-k="${k}"]`, st[k]);
    }

    $("door").classList.toggle("on", !!st.door);
    $("vibl").classList.toggle("on", !!st.vib_l);
    $("vibr").classList.toggle("on", !!st.vib_r);
    $("atc").textContent = st.atc ?? 0;
    $("speed").textContent = st.speed ?? 0;

    if (typeof st.output === "boolean") $("outEnable").checked = st.output;
    if (typeof st.disable_feedback === "boolean") $("disableFb").checked = st.disable_feedback;
    if (st.push) $("pushMode").value = st.push;
    if (typeof st.hold === "number") $("holdMs").value = st.hold;
    if (st.device_kind) $("deviceKind").value = st.device_kind;
    if (st.game_preset) $("gamePreset").value = st.game_preset;

    if (st.keys) {
      for (const [id] of KEY_FIELDS) {
        const el = $("key_" + id);
        if (el && typeof st.keys[id] === "string" && document.activeElement !== el) {
          el.value = st.keys[id];
          lastKeys[id] = st.keys[id];
        }
      }
    }
  }

  // init
  setTheme(theme);
  $("lang").value = lang;
  buildKeys();
  buildBits();
  renderStacks(0, 0, 5, 9);
  applyI18n();

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
  $("btnTestVib").onclick = () => post({ type: "cmd", cmd: "test_vib" });
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
