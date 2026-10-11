// Entry point: wires the modules together and switches between views.
//
//   api.js        every /api/* call
//   util.js       DOM lookup, formatting, path helpers
//   machine.js    the running machine and what the web UI can do on it
//   dashboard.js  status polling, hardware / storage meters, reboot & reset
//   files.js      SD card file browser
//   menu.js       pop-up menu for a file row's Actions button
//   editor.js     overlay editor for config files

import { $ } from "./util.js";
import { initDashboard, refreshVolumes, startStatusPolling } from "./dashboard.js";
import { initFiles, loadDir, filesHash, pathFromHash } from "./files.js";
import { initEditor } from "./editor.js";
import { loadMachine } from "./machine.js";

function route() {
  const hash = location.hash || "#/dashboard";
  const view = hash.startsWith("#/files") ? "files" : "dashboard";
  $("view-dashboard").hidden = view !== "dashboard";
  $("view-files").hidden = view !== "files";
  $("fb-drop").hidden = view !== "files";

  document.querySelectorAll(".nav-item[data-view]").forEach((el) => {
    el.classList.toggle("active", el.dataset.view === view);
  });

  if (view === "files") {
    loadDir(pathFromHash());
  } else {
    refreshVolumes();
  }
}

initDashboard();
initFiles();
initEditor();

$("qa-files").addEventListener("click", () => { location.hash = filesHash("/"); });
window.addEventListener("hashchange", route);

// The file actions depend on the machine, so know it before the first view.
await loadMachine();
route();
startStatusPolling();
