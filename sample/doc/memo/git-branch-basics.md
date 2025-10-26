---
title: "Gitのブランチ運用を最初に覚えるときのメモ"
description: "featureとmainの基本的な流れを整理"
pubDate: "2024-05-01"
heroImage: "../../assets/blog-placeholder-2.jpg"
status: publish
tags:
  - git
  - workflow
category: memo
---

社内で初めてGitを触るメンバー向けに、最低限のブランチ運用をまとめたメモです。以下の手順だけ守れば、mainを壊さずに開発を進められます。

1. `main`を最新化する: `git switch main && git pull`
2. 作業用ブランチを切る: `git switch -c feature/チケット番号`
3. コミットは小さくまとめ、本文には「何を」「なぜ」を一行ずつ書く
4. 作業が終わったら`git switch main`して`git merge --no-ff feature/...`
5. マージ後は`git branch -d feature/...`で枝を掃除

シンプルですが、履歴が一直線になるのでトラブル対応が楽になります。
