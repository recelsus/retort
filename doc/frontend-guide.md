# Frontend Integration Guide

This guide explains how to call the retort search API from a browser environment. The examples assume that the retort backend listens on `http://localhost:9000`.

## REST endpoints

- `GET /search?q=term&limit=20` — returns JSON containing search hits.
- `GET /meta` — exposes basic metadata such as `repo_commit` and `doc_count`.
- `GET /healthz` — returns `ok` when the server is healthy.

## Minimal fetch helper

```js
const API_ORIGIN = "http://localhost:9000";

export async function searchRetort(query, limit = 20, offset = 0) {
  if (!query || query.trim().length < 2) {
    return [];
  }

  const params = new URLSearchParams({
    q: query.trim(),
    limit: String(limit),
    offset: String(offset),
  });

  const response = await fetch(`${API_ORIGIN}/search?${params.toString()}`);
  if (!response.ok) {
    throw new Error(`retort search failed: ${response.status}`);
  }
  const payload = await response.json();
  return payload.hits ?? [];
}
```

This helper normalises inputs, performs the request, and returns the `hits` array.

## Debounced React example

The snippet below renders an input box that triggers a search after 200 ms of inactivity. It also highlights the selected result with the arrow keys and opens the link when you press <kbd>Enter</kbd>.

```jsx
import { useEffect, useMemo, useState } from "react";
import { searchRetort } from "./retort-client";

const DEBOUNCE_MS = 200;

export function RetortSearchBox() {
  const [query, setQuery] = useState("");
  const [hits, setHits] = useState([]);
  const [status, setStatus] = useState("Type to search retort...");
  const [activeIndex, setActiveIndex] = useState(-1);

  useEffect(() => {
    if (query.trim().length < 2) {
      setHits([]);
      setStatus("Type at least two characters.");
      setActiveIndex(-1);
      return;
    }

    const controller = new AbortController();
    const token = window.setTimeout(async () => {
      try {
        setStatus("Searching...");
        const result = await searchRetort(query, 20, 0);
        setHits(result);
        if (result.length > 0) {
          setActiveIndex(0);
          setStatus(`${result.length} result(s).`);
        } else {
          setActiveIndex(-1);
          setStatus("No matches.");
        }
      } catch (error) {
        setStatus("Search failed.");
        setHits([]);
        setActiveIndex(-1);
      }
    }, DEBOUNCE_MS);

    return () => {
      controller.abort();
      window.clearTimeout(token);
    };
  }, [query]);

  const handleKeyDown = (event) => {
    if (hits.length === 0) {
      return;
    }
    if (event.key === "ArrowDown") {
      event.preventDefault();
      setActiveIndex((index) => (index + 1) % hits.length);
    }
    if (event.key === "ArrowUp") {
      event.preventDefault();
      setActiveIndex((index) => (index - 1 + hits.length) % hits.length);
    }
    if (event.key === "Enter" && activeIndex >= 0) {
      window.open(hits[activeIndex].url, "_blank", "noopener");
    }
  };

  return (
    <div className="retort-search">
      <input
        type="text"
        value={query}
        onChange={(event) => setQuery(event.target.value)}
        onKeyDown={handleKeyDown}
        placeholder="Search documentation"
      />
      <p>{status}</p>
      <ul>
        {hits.map((hit, index) => (
          <li key={hit.url} className={index === activeIndex ? "active" : ""}>
            <a href={hit.url} target="_blank" rel="noopener noreferrer">
              {hit.title}
            </a>
            <div className="snippet" dangerouslySetInnerHTML={{ __html: hit.snippet }} />
          </li>
        ))}
      </ul>
    </div>
  );
}
```

This component uses `searchRetort` internally, manages keyboard navigation, and renders the snippet returned by the API.

## CSS hint

```css
.retort-search ul {
  list-style: none;
  padding: 0;
}

.retort-search li.active {
  background: #f0f5ff;
  border: 1px solid #0d6efd;
}
```

Add styles similar to the above to indicate the active list item. When integrating with an Astro site, place the component in a client-side island (`client:visible`) if you want the search to run in the browser.
