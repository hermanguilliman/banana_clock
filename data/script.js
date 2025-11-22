// Загрузка текущей яркости
fetch("/brightness")
  .then((r) => r.text())
  .then((val) => {
    document.getElementById("brightness").value = val;
    document.getElementById("bval").textContent = val;
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
  ).then(() => fetch("/beep?type=confirm")); // сразу звук, без alert
}

// Ручная установка
document.getElementById("setForm").onsubmit = function (e) {
  e.preventDefault();
  const params = new URLSearchParams(new FormData(this));
  fetch("/set?" + params.toString()).then(() => fetch("/beep?type=confirm"));
};

// Яркость
document.getElementById("brightness").oninput = function () {
  const val = this.value;
  document.getElementById("bval").textContent = val;
  fetch(`/brightness?value=${val}`);
};

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

const slider = document.getElementById("brightness");
const bval = document.getElementById("bval");

slider.oninput = function () {
  const val = this.value;
  const percent = (val / 7) * 100; // от 0 до 7 → 0–100%
  bval.textContent = val;

  this.style.setProperty("--val", percent + "%");

  fetch(`/brightness?value=${val}`);
};

fetch("/brightness")
  .then((r) => r.text())
  .then((val) => {
    slider.value = val;
    bval.textContent = val;
    slider.style.setProperty("--val", (val / 7) * 100 + "%");
  });
