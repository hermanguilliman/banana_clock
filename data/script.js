const clockEl = document.getElementById("clock");
const slider = document.getElementById("brightness");
const bval = document.getElementById("bval");
const toastEl = document.getElementById("toast");

function showToast(message, type = "success") {
    toastEl.textContent = message;
    toastEl.className = `toast ${type} show`;
    setTimeout(() => {
        toastEl.className = "toast";
    }, 3000);
}

async function apiGet(url) {
    try {
        const res = await fetch(url);
        if (!res.ok) throw new Error("Network error");
        return await res.text();
    } catch (e) {
        console.error(e);
        return null;
    }
}

setInterval(async () => {
    const t = await apiGet("/time");
    if (t) clockEl.textContent = t;
}, 1000);

async function syncWithClient() {
    const d = new Date();
    await apiGet(
        `/syncClient?h=${d.getHours()}&m=${d.getMinutes()}&s=${d.getSeconds()}`,
    );
    showToast("Время синхронизировано");
}

document.getElementById("setForm").onsubmit = async function (e) {
    e.preventDefault();
    const params = new URLSearchParams(new FormData(this));
    await apiGet("/set?" + params.toString());
    showToast("Время установлено");
};

let brightnessTimer = null;
slider.oninput = function () {
    const val = this.value;
    bval.textContent = val;
    this.style.setProperty("--val", (val / 7) * 100 + "%");
    clearTimeout(brightnessTimer);
    brightnessTimer = setTimeout(() => fetch(`/brightness?value=${val}`), 200);
};

async function init() {
    const brightVal = await apiGet("/brightness");
    if (brightVal !== null) {
        const v = parseInt(brightVal);
        slider.value = v;
        bval.textContent = v;
        slider.style.setProperty("--val", (v / 7) * 100 + "%");
    } else {
        slider.style.setProperty("--val", (slider.value / 7) * 100 + "%");
    }

    const chimeVal = await apiGet("/hourlyChime");
    if (chimeVal !== null) {
        document.getElementById("chimeSwitch").checked = parseInt(chimeVal) !== 0;
    }

    const startupVal = await apiGet("/startupChime");
    if (startupVal !== null) {
        document.getElementById("startupSwitch").checked = parseInt(startupVal) !== 0;
    }
}
init();

async function toggleChime() {
    const enabled = document.getElementById("chimeSwitch").checked;
    await apiGet(`/hourlyChime?value=${enabled ? 1 : 0}`);
    showToast(enabled ? "Куранты включены" : "Куранты выключены");
}

async function toggleStartupChime() {
    const enabled = document.getElementById("startupSwitch").checked;
    await apiGet(`/startupChime?value=${enabled ? 1 : 0}`);
    showToast(enabled ? "Звук включения включён" : "Звук включения выключен");
}

function playStartup() {
    fetch("/playStartup");
    showToast("Воспроизведение звука включения...");
}

function playHourly() {
    fetch("/playHourly");
    showToast("Воспроизведение курантов...");
}

function toggleTheme() {
    const isDark = document.body.getAttribute("data-theme") === "dark";
    document.body.setAttribute("data-theme", isDark ? "light" : "dark");
    document.getElementById("themeSwitch").checked = !isDark;
    localStorage.setItem("bananaTheme", isDark ? "light" : "dark");
}

if (localStorage.getItem("bananaTheme") === "light") {
    document.body.setAttribute("data-theme", "light");
    document.getElementById("themeSwitch").checked = false;
} else {
    document.getElementById("themeSwitch").checked = true;
}
