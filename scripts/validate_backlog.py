#!/usr/bin/env python3
"""Check local issue backlog integrity without external libraries or network."""
from __future__ import annotations
import json
import re
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'backlog/issues.json'
VALID_AREAS={'tooling','qa','core','controls','ai','assets','combat','gameplay','intel','drone','ui','vehicle','logistics','campaign','multiplayer','design','mobile','roblox','business'}

def main()->int:
    obj=json.loads(DATA.read_text('utf-8'))
    issues=obj['issues']; epics=obj['epics']; errs=[]
    ids=[item['id'] for item in issues]
    if len(set(ids)) != len(ids): errs.append('duplicate issue IDs')
    if len(issues)!=45: errs.append('expected 45 task issues')
    if len(epics)!=6: errs.append('expected 6 stage epics')
    if [x['stage'] for x in epics] != list(range(6)): errs.append('unexpected epic stages')
    for ix,item in enumerate(issues,1):
        i=item['id']; path=ROOT/item['body_file']
        if i != f'FOC-{ix:03d}': errs.append(f'nonsequential or malformed id: {i}')
        if not path.is_file(): errs.append(f'{i} missing {path}')
        else:
            content=path.read_text('utf-8')
            if not content.startswith(f'<!-- id:{i} -->'): errs.append(f'{i} missing ID marker')
            if content.count('- [ ]') < 3: errs.append(f'{i} missing checklist')
        if item['area'] not in VALID_AREAS: errs.append(f'{i} unknown area')
        if not 0<=item['stage']<=5: errs.append(f'{i} out-of-range stage')
        if item['priority'] not in ('P0','P1','P2'): errs.append(f'{i} invalid priority')
        for dep in item['depends_on']:
            if dep not in ids: errs.append(f'{i} unknown dependency {dep}')
            elif ids.index(dep)>=ix-1: errs.append(f'{i} dependency {dep} must precede issue')
        if len(item['acceptance'])<2: errs.append(f'{i} insufficient acceptance criteria')
        if not item['verification'].strip(): errs.append(f'{i} missing verification')
    for name in ('README.md','AGENTS.md','docs/ARCHITECTURE.md','docs/ROADMAP.md','docs/CODEX_WORKFLOW.md','docs/Fog_of_Command_GDD_v0.1.md','docs/Fog_of_Command_GDD_v0.1.docx','docs/Fog_of_Command_GDD_v0.1.pdf'):
        if not (ROOT/name).exists(): errs.append(f'missing expected file: {name}')
    if errs:
        print('FAIL:'); print('\n'.join(' - '+x for x in errs)); return 1
    print(f'OK: {len(issues)} task issues, {len(epics)} epics, all dependencies and GDD files present')
    return 0
if __name__=='__main__': sys.exit(main())
