/*
 * Copyright (C) 2013 Tomas Popela <tpopela@redhat.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

function add_app(info) {
  installer = document.getElementById("Apps2Desktop");
  if (info.isApp)
    installer.add(info.name, info.id, info.version, info.appLaunchUrl, info.enabled);
}

function enable_app(info) {
  installer = document.getElementById("Apps2Desktop");
  if (info.isApp)
    installer.enable(info.id);
}

function disable_app(info) {
  installer = document.getElementById("Apps2Desktop");
  if (info.isApp)
    installer.disable(info.id);
}

function remove_app(id) {
  installer = document.getElementById("Apps2Desktop");
  installer.remove(id);
}

document.addEventListener('DOMContentLoaded', function () {
  chrome.management.onInstalled.addListener(add_app);
  chrome.management.onUninstalled.addListener(remove_app);
  chrome.management.onEnabled.addListener(enable_app);
  chrome.management.onDisabled.addListener(disable_app);

  installer = document.getElementById("Apps2Desktop");
  chrome.management.getAll(function(info) {
    for (var i = 0; i < info.length; i++) {
        add_app(info[i]);
    }
  });
});

