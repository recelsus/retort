---
title: "Dockerコンテナのログを素早く確認する"
description: "よく使うコマンドだけ覚えておく"
pubDate: "2024-05-15"
heroImage: "../../assets/blog-placeholder-2.jpg"
status: publish
tags:
  - docker
  - operations
category: memo
---

本番障害で慌てないために、Dockerで走るサービスのログ確認コマンドをメモしておきます。

```bash
docker ps --format "table {{.Names}}\t{{.Status}}"
```

稼働中のコンテナを一覧し、対象が決まったら次のコマンドで追いかけます。

```bash
docker logs -f --tail 200 サービス名
```

`-f`でフォロー、`--tail`で直近の行数を絞ると読みやすくなります。ログが多いときは`--since 10m`などで期間を絞るのも効果的です。
