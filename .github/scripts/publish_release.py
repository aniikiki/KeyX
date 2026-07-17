#!/usr/bin/env python3

import json
import os
import pathlib
import re
import sys
import urllib.error
import urllib.parse
import urllib.request


API_VERSION = "2022-11-28"


class GitHubApiError(RuntimeError):
    def __init__(self, status, message):
        super().__init__(f"GitHub API 返回 {status}: {message}")
        self.status = status


def request(token, method, url, data=None, content_type="application/vnd.github+json"):
    headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "X-GitHub-Api-Version": API_VERSION,
    }
    if data is not None:
        headers["Content-Type"] = content_type

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req) as response:
            body = response.read()
            return json.loads(body) if body else None
    except urllib.error.HTTPError as error:
        body = error.read().decode("utf-8", errors="replace")
        try:
            message = json.loads(body).get("message", body)
        except json.JSONDecodeError:
            message = body
        raise GitHubApiError(error.code, message) from error


def get_context():
    return (
        os.environ["GITHUB_TOKEN"],
        os.environ["GITHUB_REPOSITORY"],
        os.environ["GITHUB_REF_NAME"],
        os.environ["GITHUB_SHA"],
    )


def validate_release():
    token, repository, tag, sha = get_context()
    makefile = pathlib.Path("ovl-KeyX/Makefile").read_text(encoding="utf-8")
    match = re.search(r"^\s*APP_VERSION\s*:=\s*(\S+)", makefile, re.MULTILINE)
    if not match:
        raise SystemExit("无法从 ovl-KeyX/Makefile 读取版本号")

    version = match.group(1)
    if tag != f"v{version}":
        raise SystemExit(f"标签 {tag} 与项目版本 {version} 不匹配")

    api_base = f"https://api.github.com/repos/{repository}"
    compare = request(token, "GET", f"{api_base}/compare/{sha}...main")
    if compare.get("status") not in {"ahead", "identical"}:
        raise SystemExit("标签提交不属于 main 分支历史")

    env_file = pathlib.Path(os.environ["GITHUB_ENV"])
    with env_file.open("a", encoding="utf-8") as file:
        file.write(f"VERSION={version}\n")
    print(f"验证通过: {tag} -> {sha}")


def publish_release(asset_name):
    token, repository, tag, sha = get_context()
    asset_path = pathlib.Path(asset_name)

    if not asset_path.is_file():
        raise SystemExit(f"发布文件不存在: {asset_path}")

    api_base = f"https://api.github.com/repos/{repository}"
    encoded_tag = urllib.parse.quote(tag, safe="")

    try:
        release = request(token, "GET", f"{api_base}/releases/tags/{encoded_tag}")
    except GitHubApiError as error:
        if error.status != 404:
            raise
        payload = json.dumps({
            "tag_name": tag,
            "target_commitish": sha,
            "name": tag,
            "generate_release_notes": True,
            "draft": False,
            "prerelease": False,
        }).encode("utf-8")
        release = request(token, "POST", f"{api_base}/releases", payload)

    for asset in release.get("assets", []):
        if asset.get("name") == asset_path.name:
            request(token, "DELETE", f"{api_base}/releases/assets/{asset['id']}")

    upload_base = release["upload_url"].split("{", 1)[0]
    asset_name = urllib.parse.quote(asset_path.name)
    request(
        token,
        "POST",
        f"{upload_base}?name={asset_name}",
        asset_path.read_bytes(),
        "application/zip",
    )
    print(f"发布完成: {release['html_url']}")


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "validate":
        validate_release()
        return
    if len(sys.argv) == 3 and sys.argv[1] == "publish":
        publish_release(sys.argv[2])
        return
    raise SystemExit(
        "用法: publish_release.py validate | publish <zip文件>"
    )


if __name__ == "__main__":
    main()
