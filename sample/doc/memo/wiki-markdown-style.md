---
title: "社内Wiki用Markdownスタイル"
description: "読みやすく保守しやすい書き方"
pubDate: "2024-05-22"
heroImage: "../../assets/blog-placeholder-2.jpg"
status: publish
tags:
  - documentation
  - wiki
category: memo
---

Wikiを書くときに気を付けているMarkdownルールをまとめました。

## 箇条書きは短文で

1行1メッセージを意識します。長い説明は段落に回し、リストはチェック項目として使います。

## コードは必ず言語を指定

```bash
curl -sSf https://example.com/healthz
```

言語を付けるだけでハイライトが効き、読み手の理解が一段と早くなります。

## 注意点は>で強調

> 本番に適用する前にステージングで検証すること。

このように引用ブロックを使うと、後から読んだときに目につきやすくなります。

些細ですが、全員が同じ書き方をすればWikiがドキュメントとして機能し続けます。
