// The C++ server serves this page, so the API is on the same address.
// (If index.html is opened directly from disk, requests go to localhost:8080.)
const API = location.port === "8080" ? "/api" : "http://localhost:8080/api";

const $ = (id) => document.getElementById(id);

// ===== Helpers =====

// Escape text so it is safe to put inside HTML
function esc(text) {
  const map = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
  return String(text ?? "").replace(/[&<>"]/g, (c) => map[c]);
}

// Small popup message
function toast(message, isError) {
  const box = $("toast");
  box.textContent = message;
  box.className = isError ? "err" : "";
  box.style.display = "block";
  setTimeout(() => {
    box.style.display = "none";
  }, 2500);
}

// Talk to the C++ backend
async function call(path, method = "GET", form) {
  try {
    const options = { method };
    if (form) {
      options.body = new URLSearchParams(new FormData(form));
    }

    const response = await fetch(API + path, options);
    const data = await response.json();

    if (!response.ok) {
      throw new Error(data.error || "Request failed");
    }
    return data;
  } catch (error) {
    const message =
      error.message === "Failed to fetch"
        ? "Cannot reach the server. Is the C++ backend running?"
        : error.message;
    toast(message, true);
    return null;
  }
}

// ===== Showing data =====

// Fill one table with rows
function fillTable(tbodyId, list, columns, kind) {
  if (list.length === 0) {
    $(tbodyId).innerHTML =
      `<tr><td class="empty" colspan="${columns.length + 1}">` +
      "Nothing here yet. Add the first entry above.</td></tr>";
    return;
  }

  $(tbodyId).innerHTML = list
    .map((item) => {
      const cells = columns.map((c) => `<td>${esc(item[c])}</td>`).join("");
      const deleteBtn =
        `<td><button class="del" onclick="removeItem('${kind}', ${item.id})">` +
        "Delete</button></td>";
      return `<tr>${cells}${deleteBtn}</tr>`;
    })
    .join("");
}

// Fill a dropdown list
function fillSelect(selectId, list) {
  $(selectId).innerHTML = list
    .map((item) => `<option value="${item.id}">${esc(item.name)}</option>`)
    .join("");
}

// Load everything from the backend and refresh the page
async function loadAll() {
  const query = encodeURIComponent($("search").value);

  const [patients, doctors, appointments] = await Promise.all([
    call("/patients?q=" + query),
    call("/doctors"),
    call("/appointments"),
  ]);

  if (!patients || !doctors || !appointments) return;

  fillTable("tP", patients, ["id", "name", "age", "gender", "disease"], "patients");
  fillTable("tD", doctors, ["id", "name", "speciality"], "doctors");
  fillTable("tA", appointments, ["id", "patient", "doctor", "date"], "appointments");

  $("cP").textContent = patients.length;
  $("cD").textContent = doctors.length;
  $("cA").textContent = appointments.length;

  fillSelect("selP", patients);
  fillSelect("selD", doctors);
}

// ===== Actions =====

async function removeItem(kind, id) {
  if (!confirm("Delete this record?")) return;

  const result = await call(`/${kind}/${id}`, "DELETE");
  if (result) {
    toast("Deleted");
    loadAll();
  }
}

// Connect a form to an API path
function setupForm(formId, path, successMessage) {
  $(formId).addEventListener("submit", async (event) => {
    event.preventDefault();

    const result = await call(path, "POST", event.target);
    if (result) {
      toast(successMessage);
      event.target.reset();
      loadAll();
    }
  });
}

// Sidebar tab switching
function setupTabs() {
  const buttons = document.querySelectorAll("nav button");
  const sections = document.querySelectorAll("main section");

  buttons.forEach((button) => {
    button.addEventListener("click", () => {
      buttons.forEach((b) => b.classList.toggle("active", b === button));
      sections.forEach((s) => {
        s.hidden = s.id !== button.dataset.tab;
      });
    });
  });
}

// ===== Start =====
setupForm("fP", "/patients", "Patient added");
setupForm("fD", "/doctors", "Doctor added");
setupForm("fA", "/appointments", "Appointment booked");
setupTabs();
$("search").addEventListener("input", loadAll);
loadAll();
