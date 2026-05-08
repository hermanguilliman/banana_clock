// Загрузка текущей яркости
fetch("/brightness")
  .then((r) => r.text())
  .then((val) => {
    document.getElementById("brightness").value = val;
    document.getElementById("bval").textContent = val;
  });

// Загрузка настроек мелодий и анимаций
fetch("/getSettings")
  .then((r) => r.json())
  .then((data) => {
    if (data.sm !== undefined) document.getElementById("sm").value = data.sm;
    if (data.sa !== undefined) document.getElementById("sa").value = data.sa;
    if (data.hm !== undefined) document.getElementById("hm").value = data.hm;
    if (data.ha !== undefined) document.getElementById("ha").value = data.ha;
  });

// Обновление часов
setInterval(() => {
  fetch("/time")
    .then((r) => r.text())
    .then((t) => {
      document.getElementById("clock").textContent = t;
    });
}, 1000);

// Синхронизация с клиентом
function syncWithClient() {
  const d = new Date();
  fetch(
    `/syncClient?h=${d.getHours()}&m=${d.getMinutes()}&s=${d.getSeconds()}`
  ).then(() => fetch("/beep?type=confirm"));
}

// Ручная установка
document.getElementById("setForm").onsubmit = function (e) {
  e.preventDefault();
  const params = new URLSearchParams(new FormData(this));
  fetch("/set?" + params.toString()).then(() => fetch("/beep?type=confirm"));
};

// Яркость
const slider = document.getElementById("brightness");
const bval = document.getElementById("bval");

slider.oninput = function () {
  const val = this.value;
  bval.textContent = val;
  this.style.setProperty("--val", (val / 7) * 100 + "%");
  fetch(`/brightness?value=${val}`);
};

fetch("/brightness")
  .then((r) => r.text())
  .then((val) => {
    slider.value = val;
    bval.textContent = val;
    slider.style.setProperty("--val", (val / 7) * 100 + "%");
  });

// Сохранение настроек мелодий и анимаций
function saveSettings() {
  const sm = document.getElementById("sm").value;
  const sa = document.getElementById("sa").value;
  const hm = document.getElementById("hm").value;
  const ha = document.getElementById("ha").value;
  
  fetch(`/saveSettings?sm=${sm}&sa=${sa}&hm=${hm}&ha=${ha}`)
    .then(() => fetch("/beep?type=confirm"))
    .then(() => alert("Настройки сохранены!"));
}

// Тестовое проигрывание
function playTest(type) {
  let m, a;
  if (type === 'startup') {
    m = document.getElementById("sm").value;
    a = document.getElementById("sa").value;
  } else {
    m = document.getElementById("hm").value;
    a = document.getElementById("ha").value;
  }
  fetch(`/playTest?m=${m}&a=${a}`);
}

// Переключение темы
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