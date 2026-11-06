#!/usr/bin/env bash
# Uploads a branch's auto-battler Release build to R2 and registers it with the dashboard (docs/specs/build_dashboard.md).
# The dashboard opens it as the game and as the editor (?editor); the artifact's Debug build is not published.
# Run by CI (.github/workflows/build.yml, the publish jobs):
#   tools/dashboard/publish.sh <dir>     <dir> holds the install of release/ (the autobattler-web artifact; its debug/ is skipped)
# Environment: BRANCH, SHA, REPO, DEFAULT_BRANCH, GH_TOKEN (for gh), CLOUDFLARE_API_TOKEN, CLOUDFLARE_ACCOUNT_ID,
# DASHBOARD_URL, DASHBOARD_TOKEN. R2_TARGET=--local writes to `wrangler dev`'s bucket instead, for a local test.
set -euo pipefail

WRANGLER=(npx --yes wrangler@4.150.0)
BUCKET=nv-builds
dir=$1
folder=${BRANCH//\//-}

config=release
test -f "$dir/$config/index.html"
while IFS= read -r -d '' path; do
  file=${path#"$dir/$config/"}
  case "$file" in
    *.html) type="text/html; charset=utf-8" ;;
    *.js) type=text/javascript ;;
    *.wasm) type=application/wasm ;;
    *.json) type=application/json ;;
    *) type=application/octet-stream ;;
  esac
  "${WRANGLER[@]}" r2 object put "$BUCKET/builds/$folder/$config/$file" --file "$path" --content-type "$type" "${R2_TARGET:---remote}"
done < <(find "$dir/$config" -type f -print0)

# Sizes: what a visitor downloads, and everything the folder keeps (the same, now that it holds one build).
release=$(find "$dir/$config" -type f -printf '%s\n' | awk '{ s += $1 } END { print s + 0 }')
stored=$release

subject=$(gh api "repos/$REPO/commits/$SHA" --jq '.commit.message | split("\n")[0]')
date=$(gh api "repos/$REPO/commits/$SHA" --jq '.commit.committer.date')
if [ "$BRANCH" = "$DEFAULT_BRANCH" ]; then
  ahead=0
else
  # Commits master does not have; the dashboard counts a merged pull request's branch as in master.
  ahead=$(gh api "repos/$REPO/compare/$DEFAULT_BRANCH...$SHA" --jq '.ahead_by')
fi

# TODO: drop `debug: 0` once the Worker on master takes records without it (state.mjs ignores it since 2026-11-06); until then
# an older Worker refuses a record that lacks it.
jq -n --arg branch "$BRANCH" --arg sha "$SHA" --arg subject "$subject" --arg date "$date" \
  --argjson release "$release" --argjson stored "$stored" --argjson ahead "$ahead" \
  '{ branch: $branch, sha: $sha, subject: $subject, date: $date, release: $release, stored: $stored, ahead: $ahead, debug: 0 }' |
  curl -fsS -X POST "$DASHBOARD_URL/api/builds" -H "Authorization: Bearer $DASHBOARD_TOKEN" -H "Content-Type: application/json" --data @-
echo "Published $BRANCH at $SHA: $DASHBOARD_URL/builds/$folder/release/ (the editor: ?editor)"
