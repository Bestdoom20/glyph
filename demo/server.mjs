// GLYPH demo server — static files + /api/chat proxy (key stays server-side).
import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { join, extname } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = fileURLToPath(new URL('.', import.meta.url));
const PORT = process.env.PORT || 8199;
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };

http.createServer(async (req, res) => {
  if (req.method === 'POST' && req.url === '/api/chat') return chat(req, res);
  const path = req.url === '/' ? '/index.html' : req.url.split('?')[0];
  try {
    const body = await readFile(join(ROOT, path.replace(/\.\./g, '')));
    res.writeHead(200, { 'content-type': MIME[extname(path)] || 'application/octet-stream' });
    res.end(body);
  } catch {
    res.writeHead(404); res.end('not found');
  }
}).listen(PORT, '0.0.0.0', () => console.log(`GLYPH demo on :${PORT}`));

async function chat(req, res) {
  let raw = '';
  for await (const c of req) { raw += c; if (raw.length > 10000) { res.writeHead(413); return res.end(); } }
  let prompt;
  try { prompt = JSON.parse(raw).prompt; } catch { /* fall through */ }
  if (typeof prompt !== 'string' || !prompt.trim() || prompt.length > 2000) {
    res.writeHead(400, { 'content-type': 'application/json' });
    return res.end(JSON.stringify({ error: 'prompt must be a non-empty string ≤2000 chars' }));
  }
  const key = process.env.NEXUS_MASTER_KEY;
  if (!key) {
    res.writeHead(503, { 'content-type': 'application/json' });
    return res.end(JSON.stringify({ error: 'NEXUS_MASTER_KEY not set on server' }));
  }
  try {
    const r = await fetch('http://localhost:8000/adler/complete', {
      method: 'POST',
      headers: { 'X-API-Key': key, 'content-type': 'application/json' },
      body: JSON.stringify({ task: prompt, budget_limit: 0.02 }),
      signal: AbortSignal.timeout(60000),
    });
    const d = await r.json();
    if (!r.ok || !d.content) throw new Error(d.error || d.detail || `nexus ${r.status}`);
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ content: d.content, model: d.model_key, cost: d.cost_usd }));
  } catch (e) {
    res.writeHead(502, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ error: String(e.message || e) }));
  }
}
