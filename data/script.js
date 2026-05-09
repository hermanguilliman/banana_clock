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

slider.oninput = function () {
    const val = this.value;
    bval.textContent = val;
    this.style.setProperty("--val", (val / 7) * 100 + "%");
    fetch(`/brightness?value=${val}`);
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

    try {
        const res = await fetch("/getSettings");
        if (res.ok) {
            const settings = await res.json();
            if (settings) {
                if (settings.sm !== undefined)
                    document.getElementById("sm").value = settings.sm;
                if (settings.sa !== undefined)
                    document.getElementById("sa").value = settings.sa;
                if (settings.hm !== undefined)
                    document.getElementById("hm").value = settings.hm;
                if (settings.ha !== undefined)
                    document.getElementById("ha").value = settings.ha;
            }
        }
    } catch (e) {
        console.error(e);
    }
}
init();

async function saveSettings() {
    const sm = document.getElementById("sm").value;
    const sa = document.getElementById("sa").value;
    const hm = document.getElementById("hm").value;
    const ha = document.getElementById("ha").value;
    await apiGet(`/saveSettings?sm=${sm}&sa=${sa}&hm=${hm}&ha=${ha}`);
    showToast("Настройки сохранены в память");
}

function playTest(type) {
    let m, a;
    if (type === "startup") {
        m = document.getElementById("sm").value;
        a = document.getElementById("sa").value;
    } else {
        m = document.getElementById("hm").value;
        a = document.getElementById("ha").value;
    }
    fetch(`/playTest?m=${m}&a=${a}`);
    showToast("Воспроизведение...", "success");
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
