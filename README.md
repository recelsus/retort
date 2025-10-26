# retort search server

A lightweight C++ backend that provides full-text search over Astro Markdown (MD/MDX) content. The project bundles a sample dataset under `sample/` and a static HTML page that behaves like an incremental finder.

## Quick start

```bash
cmake -S . -B build
cmake --build build
./sample/test.sh
```

The helper script rebuilds the sample index and launches `retort serve` on `127.0.0.1:9000`. Stop the server with <kbd>Ctrl</kbd>+<kbd>C</kbd>.

To explore the demo UI, open `sample/index.html` in a browser. It will query `http://localhost:9000/search` as you type.

## Frontend integration

For a full overview see [`doc/frontend-guide.md`](doc/frontend-guide.md). In short:

1. Issue `GET /search?q=term&limit=20` from your frontend code.
2. Debounce user input to avoid flooding the backend.
3. Render the `snippet` field as HTML and the rest as plain text.

The guide includes a ready-to-use fetch helper and a React example that mimics the behaviour of the bundled demo page.

## Writing new indexes

```
./build/retort write --src_dir path/to/content --out path/to/index.sqlite
```

If you work inside this repository, run `./sample/test.sh` to regenerate `sample/sample_index.sqlite` and restart the bundled server in one go.

## Folder structure

- `src/` – CLI, writer, and HTTP server source files
- `sample/` – example content and the static HTML demo
- `doc/` – integration guides and additional documentation

## License

MIT-like for now; adapt to your team’s needs.
