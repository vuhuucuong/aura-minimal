// Local-only static server. No dependencies. Run: node server.cjs
const http = require('http');
const fs = require('fs');
const path = require('path');
const root = path.join(__dirname, 'website');
const port = Number(process.env.AURA_PORT || 8080);
const types = {'.html':'text/html; charset=utf-8','.css':'text/css; charset=utf-8','.js':'text/javascript; charset=utf-8','.json':'application/json','.svg':'image/svg+xml','.png':'image/png','.bin':'application/octet-stream','.zip':'application/zip','.txt':'text/plain; charset=utf-8'};
const server = http.createServer((req,res) => {
  if (!['GET','HEAD'].includes(req.method)) { res.writeHead(405, {'Allow':'GET, HEAD'}); return res.end(); }
  let pathname;
  try { pathname = decodeURIComponent(new URL(req.url,'http://localhost').pathname); }
  catch { res.writeHead(400); return res.end('Bad request'); }
  const file = path.resolve(root, '.' + (pathname === '/' ? '/index.html' : pathname));
  if (!file.startsWith(root + path.sep)) { res.writeHead(403); return res.end('Forbidden'); }
  fs.stat(file, (error, stat) => {
    if (error || !stat.isFile()) { res.writeHead(404); return res.end('Not found'); }
    res.writeHead(200, {'Content-Type':types[path.extname(file)] || 'application/octet-stream','Content-Length':stat.size,'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Permissions-Policy':'serial=(self)'});
    if (req.method === 'HEAD') return res.end();
    fs.createReadStream(file).pipe(res);
  });
});
server.on('error',error => { console.error(error.code === 'EADDRINUSE' ? `Port ${port} is already in use. Close the other server or set AURA_PORT to a different port.` : error.message); process.exit(1); });
server.listen(port, '127.0.0.1', () => console.log(`Aura Minimal installer: http://localhost:${port}\nOpen in desktop Chrome or Edge. Press Ctrl+C to stop.`));
