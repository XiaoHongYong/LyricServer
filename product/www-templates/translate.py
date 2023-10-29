# encoding=utf-8

import os
import re
import json
import shutil

from collections import OrderedDict


NOT_TRANSLATED = '--TODO--'
global_language_trans = {}

def read_file(fn):
    with open(fn, 'rb') as fp:
        return fp.read().decode('utf-8')

def write_file(fn, data):
    if type(data) is str:
        data = data.encode('utf-8')

    with open(fn, 'wb') as fp:
        fp.write(data)

def write_file_on_cmp(fn, data):
    if type(data) is str:
        data = data.encode('utf-8')

    if os.path.exists(fn) and read_file(fn) == data:
        return
    write_file(fn, data)

def get_file_ext(name):
    return os.path.splitext(name)[1]

def extract_lang_fn(fn, strings):
    '''
    从 @fn 中提取翻译的 string, 返回文件内容格式为 [ text, to_trans_text, text, to_trans_text, ... ]
    '''

    text = read_file(fn)
    parts = text.split('_TLM(')
    arr = [parts[0]]
    for i in range(1, len(parts)):
        part = parts[i]
        quote = part[0]
        if quote == '"' or quote == '\'':
            part = part[1:]
            string, _, remain = part.partition(quote + ')')
            arr[-1] += quote
            remain = quote + remain
        else:
            string, _, remain = part.partition(')')
            if '(' in string:
                # 如果翻译的字符中有 '('，则 使用 ')_TLM' 为结束标志
                string, _, remain = part.partition(')_TLM')

        arr.append(string)
        arr.append(remain)

        strings[string] = ''

    return arr


def read_translate_fn(fn):
    '''
    读取 translate 文件，其格式为：
translate_from_text
    =translate_to_text
    '''
    d = OrderedDict()

    if os.path.exists(fn):
        content = read_file(fn)
        m = re.findall(r'(.*?)\n\s+=(.*?)\n+', content)

        for k, v in m:
            d[k] = v

    return d

def enum_all_trans_files(path):
    files = []
    for root, _, names in os.walk(path):
        for name in names:
            fn = os.path.join(root, name)
            if fn.startswith('.'):
                continue
            if fn.endswith('.html') or fn.endswith('.js') or fn.endswith('.lua'):
                files.append(fn)

    return files

def make_translation(path_out):
    '''
    从待翻译的目录 @path 中提取翻译的字符串，根据 languages.json 中指定的语言，创建对应的翻译目录
    languages.json: 定义了需要翻译的语言
    en.txt, zh-cn.txt 等包含了要翻译的字符串内容
    en, zh-cn 等目录为自动创建
    '''
    print(' 根据翻译模版创建翻译文件 '.center(70, '='))
    path_src = path_out + '_src'
    print('翻译: ' + path_src)

    count_created = 0
    count_modified = 0
    count_removed = 0

    def write_file_on_cmp(fn, data):
        if type(data) is str:
            data = data.encode('utf-8')

        if os.path.exists(fn):
            with open(fn, 'rb') as fp:
                if fp.read() == data:
                    return
            nonlocal count_modified
            count_modified += 1
        else:
            nonlocal count_created
            count_created += 1
        write_file(fn, data)

    strings = OrderedDict()
    strings['__language__'] = ''
    strings['__contact__'] = ''

    assert(path_out != '/')
    if os.path.exists(path_out):
        existing_out_files = set(enum_all_trans_files(path_out))
    else:
        os.makedirs(path_out)
        existing_out_files = set()

    # 提取 lua 中的翻译字符串
    path_lua = os.path.join(path_out, '../../lua')
    for fn in enum_all_trans_files(path_lua):
        extract_lang_fn(fn, strings)

    # 提取 html 中的翻译字符串
    path_org = os.path.join(path_src, 'org')
    # files: [(related_file_name, file_parts)]
    files = {fn : extract_lang_fn(fn, strings) for fn in enum_all_trans_files(path_org)}


    fn_languages = os.path.join(path_src, 'languages.json')
    shutil.copyfile(fn_languages, os.path.join(path_out, 'languages.json'))

    # 根据 languages.json 中定义的语言，生成翻译结果
    langs = json.loads(read_file(fn_languages))
    for lang, lang_desc in langs.items():
        trans_fn = os.path.join(path_src, lang + '.txt')
        trans_json_fn = os.path.join(path_out, lang + '.json')
        trans = read_translate_fn(trans_fn)
        trans['__language__'] = lang_desc

        global_trans = global_language_trans.get(lang) or {}

        count_untranslated = 0
        new_trans = OrderedDict()
        for k in strings.keys():
            v = trans.get(k)
            if v is None or v == NOT_TRANSLATED:
                v = global_trans.get(k) or NOT_TRANSLATED
            new_trans[k] = v
            count_untranslated += (v == NOT_TRANSLATED)

        if count_untranslated > 0:
            print(f'{lang}: 未翻译文字串: {count_untranslated}')

        a = [f'{k}\n     ={v}' for k, v in new_trans.items()]
        write_file_on_cmp(trans_fn, '\n\n'.join(a) + '\n')
        write_file_on_cmp(trans_json_fn, json.dumps(new_trans, indent=1))

        for fn, file_parts in files.items():
            name = fn[len(path_org) + 1:]
            if name[0] == os.path.sep:
                name = name[1:]
            fn_dst = os.path.join(path_out, lang, name)
            path = os.path.dirname(fn_dst)
            if not os.path.exists(path):
                os.makedirs(path)

            arr = []
            translate = False
            for part in file_parts:
                if translate:
                    v = new_trans.get(part)
                    if not v or v == NOT_TRANSLATED:
                        v = part
                    arr.append(v)
                else:
                    arr.append(part)
                translate = not translate

            write_file_on_cmp(fn_dst, ''.join(arr))
            if fn_dst in existing_out_files:
                existing_out_files.remove(fn_dst)

    # 删除多余的文件
    for fn in existing_out_files:
        os.remove(fn)
        count_removed += 1

    print(f'总共修改了 {count_modified} 个文件, 删除了 {count_removed} 个文件, 创建了 {count_created} 个文件')

def expand_global_translate(path_src):
    fn_languages = os.path.join(path_src, 'languages.json')

    langs = json.loads(read_file(fn_languages))
    for lang in langs.keys():
        trans_fn = os.path.join(path_src, lang + '.txt')
        trans = read_translate_fn(trans_fn)

        global_trans = global_language_trans.get(lang)
        if global_trans is None:
            global_trans = global_language_trans[lang] = {}

        for k, v in trans.items():
            v1 = global_trans.get(k)
            if v1 is None or v1 == NOT_TRANSLATED:
                global_trans[k] = v

if __name__ == '__main__':
    path = os.path.dirname(os.path.abspath(__file__))

    # 提取全局的翻译字符串，用于合并翻译结果.
    expand_global_translate(os.path.join(path, 'viewlyrics_src'))
    expand_global_translate(os.path.join(path, 'crintsoft_src'))

    make_translation(os.path.join(path, 'viewlyrics'))
    make_translation(os.path.join(path, 'crintsoft'))
