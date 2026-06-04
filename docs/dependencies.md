---
title: Dependencies
description: Manage native dependencies with vcpkg manifests.
---

# Dependencies

The generated app uses `vcpkg.json` to declare native dependencies.

```json
{
  "dependencies": [
    "drogon",
    "jsoncpp"
  ]
}
```

Future CLI work should let `xp run` install or bootstrap these dependencies automatically.
