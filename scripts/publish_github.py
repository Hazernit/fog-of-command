#!/usr/bin/env python3
"""Create/push a private repo and import prepared issues via GitHub CLI.

No GitHub token is embedded. Requires `gh auth login`, git, gh; optional Git LFS.
Idempotent issue import by stable [FOC-###] title prefix; never force-pushes.
"""
from __future__ import annotations
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
META=json.loads((ROOT/'backlog/issues.json').read_text(encoding='utf-8'))
LABELS={
    'type:epic':('5319E7','Большой этап / epic'),
    'type:task':('0E8A16','Исполнимая задача'),
    'type:bug':('D73A4A','Воспроизводимая ошибка'),
    'type:proposal':('BFDADC','Предложение механики'),
    'priority:P0':('B60205','Блокирует ближайший этап'),
    'priority:P1':('D93F0B','Следующая очередь'),
    'priority:P2':('FBCA04','Отложено / эксперименты'),
    'status:deferred':('EDEDED','Не передавать Codex до решения владельца'),
}
for s in range(6):
    LABELS[f'stage:{s}']=('0052CC' if s<4 else 'C5DEF5',f'Roadmap stage S{s}')
for item in META['issues']:
    LABELS[f'area:{item["area"]}']=('E4E4E4',f'Область: {item["area"]}')

def run(args:list[str], *, check:bool=True, capture:bool=True)->subprocess.CompletedProcess[str]:
    result=subprocess.run(args,cwd=ROOT,text=True,encoding='utf-8',errors='replace',stdout=subprocess.PIPE if capture else None,stderr=subprocess.PIPE if capture else None)
    if check and result.returncode:
        raise RuntimeError(f'Command failed: {" ".join(args)}\n{result.stderr or result.stdout}')
    return result

def issue_key(title:str)->str|None:
    m=re.match(r'^\[(FOC-\d{3}|EPIC-S\d)\]', title)
    return m.group(1) if m else None

def git_setup(repo_full:str,visibility:str)->None:
    if not (ROOT/'.git').exists():
        run(['git','init','-b','main'])
    if shutil.which('git-lfs') or run(['git','lfs','version'],check=False).returncode==0:
        run(['git','lfs','install','--local'])
    else:
        print('NOTE: Git LFS not installed. Install before committing Unreal .uasset/.umap later.')
    run(['git','add','--all'])
    staged=run(['git','diff','--cached','--quiet'],check=False)
    if staged.returncode not in (0,1): raise RuntimeError('Cannot inspect staged changes')
    if staged.returncode==1:
        run(['git','commit','-m','docs: initialize Fog of Command GDD and Codex backlog'])
    branch=run(['git','branch','--show-current']).stdout.strip()
    if branch!='main': raise RuntimeError(f'Expected current branch main, got {branch}. Refusing to switch branch automatically.')
    run(['gh','auth','setup-git'])
    existing_origin=run(['git','remote','get-url','origin'],check=False)
    target=f'https://github.com/{repo_full}.git'
    if existing_origin.returncode==0:
        url=existing_origin.stdout.strip()
        expected_urls={target,f'git@github.com:{repo_full}.git',f'https://github.com/{repo_full}'}
        if url not in expected_urls:
            raise RuntimeError(f'origin points elsewhere: {url}. Refusing to push to wrong repository.')
    present=run(['gh','repo','view',repo_full,'--json','name'],check=False)
    if present.returncode==0:
        print(f'Existing repository detected: {repo_full}')
        if existing_origin.returncode != 0:
            run(['git','remote','add','origin',target])
        run(['git','push','--set-upstream','origin','main'])
    else:
        cmd=['gh','repo','create',repo_full,f'--{visibility}','--description',
             'Fog of Command — RTS + intelligence uncertainty + direct soldier/drone control; pre-production',
             '--source','.','--remote','origin','--push']
        run(cmd)
    print('Pushed:',f'https://github.com/{repo_full}')

def existing_issues(repo:str)->dict[str,int]:
    r=run(['gh','issue','list','--repo',repo,'--state','all','--limit','1000','--json','number,title'])
    issues=json.loads(r.stdout)
    out={}
    for it in issues:
        key=issue_key(it.get('title',''))
        if key: out[key]=int(it['number'])
    return out

def create_one(repo:str,key:str,title:str,body_path:Path,labels:list[str], existing:dict[str,int])->int:
    if key in existing:
        print(f'SKIP existing {key}: #{existing[key]}')
        return existing[key]
    cmd=['gh','issue','create','--repo',repo,'--title',title,'--body-file',str(body_path)]
    for label in labels: cmd.extend(['--label',label])
    resp=run(cmd,check=True)
    m=re.search(r'/issues/(\d+)',resp.stdout)
    if not m: raise RuntimeError(f'Created {key} but could not parse Issue URL: {resp.stdout}')
    n=int(m.group(1)); existing[key]=n
    print(f'CREATED {key}: {repo}#{n}')
    return n

def import_issues(repo:str)->None:
    for label,(color,description) in sorted(LABELS.items()):
        run(['gh','label','create',label,'--repo',repo,'--color',color,'--description',description,'--force'])
    existing=existing_issues(repo)
    epics={}
    for epic in META['epics']:
        title=f'[{epic["id"]}] {epic["title"]}'
        related=[x for x in META['issues'] if x['stage']==epic['stage']]
        text=(f'## Цель\n{epic["description"]}\n\n'
              + ('**Отложено. Не поручать Codex без отдельного решения владельца.**\n\n' if epic['stage']>=4 else '')
              + '## Подзадачи\n'+''.join(f'- [ ] `{x["id"]}` — {x["title"]}\n' for x in related)
              + '\n## Gate\nСм. `docs/ROADMAP.md`; этап закрывается только после проверяемой демонстрации, не по факту создания задач.\n')
        from tempfile import NamedTemporaryFile
        with NamedTemporaryFile(mode='w',encoding='utf-8',suffix='.md',delete=False) as f:
            f.write(text); temp=Path(f.name)
        try:
            epics[epic['stage']]=create_one(repo,epic['id'],title,temp,['type:epic',f'stage:{epic["stage"]}'],existing)
        finally:
            temp.unlink(missing_ok=True)
    newly_created=[]
    for issue in META['issues']:
        was_known=issue['id'] in existing
        create_one(repo,issue['id'],issue['github_title'],ROOT/issue['body_file'],issue['labels'],existing)
        if not was_known: newly_created.append(issue)
    # Native sub-issues/dependency links are a convenience; import remains successful
    # when a GitHub account/repo lacks permission or CLI support for these features.
    for issue in newly_created:
        n=existing[issue['id']]
        cmd=['gh','issue','edit',str(n),'--repo',repo,'--parent',str(epics[issue['stage']])]
        for dep in issue['depends_on']:
            if dep in existing: cmd.extend(['--add-blocked-by',str(existing[dep])])
        resp=run(cmd,check=False)
        if resp.returncode:
            print(f'NOTE: Optional native parent/blocker relations for {issue["id"]} could not be set: {resp.stderr.strip()[:160]}')
    print(f'Done: {len(META["epics"])} stage epics + {len(META["issues"])} tasks exist in {repo}')

def main()->int:
    parser=argparse.ArgumentParser(description='Push Fog of Command and publish all GitHub Issues via gh CLI')
    parser.add_argument('--owner',default='Hazernit')
    parser.add_argument('--repo',default='fog-of-command')
    group=parser.add_mutually_exclusive_group()
    group.add_argument('--public',action='store_true',help='Make new repo public (default private)')
    group.add_argument('--private',action='store_true',help='Make new repo private (default)')
    parser.add_argument('--dry-run',action='store_true',help='Only show the plan. No GitHub or Git mutation')
    parser.add_argument('--skip-issues',action='store_true',help='Publish files only')
    args=parser.parse_args()
    repo=f'{args.owner}/{args.repo}'; visibility='public' if args.public else 'private'
    if args.dry_run:
        print(f'DRY RUN, no changes. Target: {repo} ({visibility})')
        print(f'{len(META["epics"])} epics, {len(META["issues"])} task issues; files + docs + scripts.')
        for epic in META['epics']:
            tasks=[x for x in META['issues'] if x['stage']==epic['stage']]
            print(f'  S{epic["stage"]}: {len(tasks)} task issues ({tasks[0]["id"]}–{tasks[-1]["id"]})')
        return 0
    if not re.fullmatch(r'[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,37})', args.owner):
        raise ValueError('Invalid GitHub owner')
    if not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9_.-]{0,98}', args.repo):
        raise ValueError('Invalid GitHub repository name')
    for program in ('git','gh'):
        if shutil.which(program) is None: raise RuntimeError(f'{program} is not installed. Read docs/PUBLISHING.md')
    run(['gh','auth','status'])
    run([sys.executable,str(ROOT/'scripts/validate_backlog.py')])
    git_setup(repo,visibility)
    if not args.skip_issues: import_issues(repo)
    return 0

if __name__=='__main__':
    try: sys.exit(main())
    except (RuntimeError,ValueError) as exc:
        print(f'ERROR: {exc}',file=sys.stderr)
        sys.exit(1)
