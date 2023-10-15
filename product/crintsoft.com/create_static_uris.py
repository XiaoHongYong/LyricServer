import os

EXCLUDE_NAMES = ['base.html']

def create_lua_static_uris(root_path, out_lua_file):
    if not root_path.endswith('/'):
        root_path += '/'
    len_root_path = len(root_path)

    files = []
    for path, _, names in os.walk(root_path):
        for name in names:
            if name.endswith('.html') and name[0] != '.' and name not in EXCLUDE_NAMES:
                fn = os.path.join(path, name)
                fn = fn[len_root_path:]
                files.append([fn[:-5], fn]) # Remove extention: '.html'
                if name == 'default.html':
                    files.append([fn[:-len(name)], fn])

    lines = ['''-- This file was created by {} automatically, do NOT modify it.

local staticPages = {{
    '''.format(os.path.basename(__file__))]
    for src, dst in files:
        # ["/minilyrics/download/older"] = 'minilyrics/download_older.html',
        lines.append('    ["/{}"] = "{}",'.format(src, dst))

    lines.append('}\n')
    lines.append('return staticPages\n')

    with open(out_lua_file, 'wb') as fp:
        fp.write('\n'.join(lines))

    print('Added {} files'.format(len(files)))
    print(' DONE '.center(60, '='))

def main():
    work_path = os.path.dirname(os.path.abspath(__file__))

    create_lua_static_uris(work_path, os.path.join(work_path, '../lua/crintsoftStaticUris.lua'))

if __name__ == '__main__':
    # path = '/Users/hongyongxiao/zikiplayer/MLServerNgx/product/crintsoft.com/minilyrics/faq/'
    # for name in os.listdir(path):
    #     if name.startswith('faq-'):
    #         new_name = name[4:]
    #         os.rename(os.path.join(path, name), os.path.join(path, new_name))
    #         print name, new_name

    main()
