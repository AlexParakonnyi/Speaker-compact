#!/usr/bin/env node
// deploy.js — заливает frontend/dist/ на устройство через /api/deploy-frontend
// Использование: node scripts/deploy.js [ip]   (ip по умолчанию 192.168.4.1)

import { readFileSync, readdirSync, statSync } from 'fs';
import { join, relative } from 'path';

const ip   = process.argv[2] ?? '192.168.4.1';
const base = new URL(`http://${ip}`);
const dist = new URL('../dist/', import.meta.url).pathname.replace(/^\/([A-Z]:)/, '$1');

function walk(dir) {
  const result = [];
  for (const name of readdirSync(dir)) {
    const full = join(dir, name);
    if (statSync(full).isDirectory()) result.push(...walk(full));
    else result.push(full);
  }
  return result;
}

const files = walk(dist);
console.log(`Деплой ${files.length} файлов → http://${ip}\n`);

let ok = 0;
for (const file of files) {
  const rel  = '/' + relative(dist, file).replace(/\\/g, '/');
  const path = '/www' + rel;
  const url  = new URL('/api/deploy-frontend', base);
  url.searchParams.set('path', path);

  const body = readFileSync(file);
  let status;
  try {
    const res = await fetch(url.toString(), { method: 'POST', body });
    status = res.status;
  } catch (e) {
    console.error(`  FAIL  ${path}  —  ${e.message}`);
    continue;
  }

  const mark = status === 200 ? '✓' : `✗ ${status}`;
  console.log(`  ${mark}  ${path}  (${body.length} B)`);
  if (status === 200) ok++;
}

console.log(`\n${ok}/${files.length} файлов залито.`);
if (ok < files.length) process.exit(1);
