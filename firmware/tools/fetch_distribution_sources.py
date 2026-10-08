"""Fetch source/notice archives for the exact ESP-IDF revision in the installed build.
Only downloads public upstream data; does not modify application/dependency sources.
"""
from pathlib import Path
import urllib.request, urllib.error, json, configparser, tarfile, hashlib, concurrent.futures, subprocess, shutil
root=Path(__file__).resolve().parents[2]
cache=root/'.distribution-cache';cache.mkdir(exist_ok=True)
idf='87912cd291'
def fetch(url, dest):
    if not dest.exists():
        partial=dest.with_suffix(dest.suffix+'.partial')
        if url.startswith('https://api.github.com/') and shutil.which('gh'):
            data=subprocess.run(['gh','api',url.removeprefix('https://api.github.com/')],capture_output=True,check=True).stdout
            partial.write_bytes(data)
        else:
            result=subprocess.run(['curl.exe','-4','-L','--retry','2','--max-time','900','-sS',url,'-o',str(partial),'--write-out','%{http_code}'],capture_output=True,text=True,check=True)
            if result.stdout.strip()!='200':
                partial.unlink(missing_ok=True)
                raise urllib.error.HTTPError(url,int(result.stdout.strip()),'Download failed',None,None)
        dest.with_suffix(dest.suffix+'.partial').replace(dest)
    return dest
treefile=fetch(f'https://api.github.com/repos/espressif/esp-idf/git/trees/{idf}?recursive=1',cache/'idf-tree.json')
tree=json.loads(treefile.read_text(encoding='utf-8-sig'))
assert not tree['truncated']
modulesfile=fetch(f'https://raw.githubusercontent.com/espressif/esp-idf/{idf}/.gitmodules',cache/'idf.gitmodules')
modules=configparser.ConfigParser(strict=False);modules.read(modulesfile,encoding='utf-8')
gitlinks={x['path']:x['sha'] for x in tree['tree'] if x['mode']=='160000'}
repos={}
for section in modules.sections():
    path=modules[section]['path'];url=modules[section]['url']
    if url.startswith('../../'):url='https://github.com/'+url[6:]
    repos[path]=url.removesuffix('.git').removeprefix('https://github.com/')
# Include sources for linked/supporting open-source components. Binary-only radio
# blobs are copied from the installed libs; retain their exact upstream licenses.
selected=['components/heap/tlsf','components/lwip/lwip','components/mbedtls/mbedtls',
          'components/bt/host/nimble/nimble','components/json/cJSON',
          'components/bootloader/subproject/components/micro-ecc/micro-ecc']
binary=['components/esp_wifi/lib','components/esp_phy/lib','components/esp_coex/lib',
        'components/bt/controller/lib_esp32c3_family']
records=[]
def source(path):
    repo= repos[path];commit=gitlinks[path];label=repo.split('/')[-1]
    dest=fetch(f'https://api.github.com/repos/{repo}/tarball/{commit}',cache/f'{label}-{commit}.tar.gz')
    with tarfile.open(dest) as archive:assert archive.getmembers()
    return {'path':path,'repo':repo,'commit':commit,'archive':dest.name,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
idfarchive=fetch(f'https://api.github.com/repos/espressif/esp-idf/tarball/{idf}',cache/f'esp-idf-{idf}.tar.gz')
with tarfile.open(idfarchive) as archive:assert archive.getmembers()
records.append({'repo':'espressif/esp-idf','commit':idf,'archive':idfarchive.name,'sha256':hashlib.sha256(idfarchive.read_bytes()).hexdigest(),'note':'Main tree; selected submodule source archives accompany it.'})
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    records.extend(pool.map(source,selected))
def notice(path):
    repo=repos[path];commit=gitlinks[path];dest=root/'legal/ESP-IDF-radio'/repo.split('/')[-1];dest.mkdir(parents=True,exist_ok=True)
    available=[]
    for name in ['LICENSE','LICENSE.txt','NOTICE']:
        try:
            p=fetch(f'https://raw.githubusercontent.com/{repo}/{commit}/{name}',dest/name)
            available.append(name)
        except urllib.error.HTTPError as error:
            if error.code!=404:raise
    assert available,repo
    return {'path':path,'repo':repo,'commit':commit,'license_files':available,'note':'Binary-only radio component for the ESP32-S3 target; exact upstream license retained.'}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:records.extend(pool.map(notice,binary))
(cache/'sources.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
print('Downloaded and checked source archives:',len(records))
