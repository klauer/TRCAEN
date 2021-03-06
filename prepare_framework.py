import argparse
import os
import itertools
import platform
import string

def remove_duplicates(the_list):
    seen = set()
    result = []
    for m in the_list:
        if m not in seen:
            result.append(m)
            seen.add(m)
    return result

def merge(x):
    return list(itertools.chain.from_iterable(x))

def collect_modules(obj):
    return filter(lambda x: isinstance(x, Module), (getattr(obj, name) for name in dir(obj)))

class Env(object):
    def __init__(self, top_path, base_path, dont_make_base=False):
        self.top_path = top_path
        
        self.base = Module(self, 'base', path=base_path, dont_generate=True,
            release_name='EPICS_BASE', dont_clean=True, dont_make=dont_make_base)
        
    def generate_makefile(self, all_modules, default_targets):
        with open(os.path.join(self.top_path, 'Makefile'), 'w') as f:
            f.write('# Generated by prepare.py, do not modify.\n\n')
            
            f.write('# Common definitions.\n\n')
            f.write('all: build_default_targets\n\n')
            f.write('DEFAULT_TARGETS = {0}\n\n'.format(' '.join(m.name for m in default_targets)))
            f.write('ALL_MODULES = {0}\n\n'.format(' '.join(m.name for m in all_modules)))
            f.write('CLEAN_MODULES = {0}\n\n'.format(' '.join(m.name for m in all_modules if not m.dont_clean)))
            f.write('.PHONY: build_default_targets\n')
            f.write('build_default_targets: $(DEFAULT_TARGETS)\n\n')
            f.write('.PHONY: clean\n')
            f.write('clean: $(foreach module,$(CLEAN_MODULES),clean-$(module))\n\n')
            f.write('\n')
            
            f.write('# Build module with dependencies.\n\n')
            for m in all_modules:
                if m.dont_make:
                    continue
                f.write('.PHONY: {0}\n'.format(m.name))
                f.write('{0}: {1}\n\t$(MAKE) -C "{2}"\n\n'.format(m.name, ' '.join(d.name for d in m.deps if not d.dont_make), m.path))
            f.write('\n')
            
            f.write('# Build module without dependencies.\n\n')
            for m in all_modules:
                if m.dont_make:
                    continue
                target_name = 'only-{0}'.format(m.name)
                f.write('.PHONY: {0}\n'.format(target_name))
                f.write('{0}:\n\t$(MAKE) -C "{1}"\n\n'.format(target_name, m.path))
            f.write('\n')
            
            f.write('# Clean module.\n\n')
            for m in all_modules:
                if m.dont_clean:
                    continue
                f.write('.PHONY: clean-{0}\n'.format(m.name))
                f.write('clean-{0}:\n'.format(m.name))
                f.write('\t$(MAKE) -C "{0}" clean distclean\n\n'.format(m.path))
            f.write('\n')

class Module(object):
    def __init__(self, env, name, path, deps=[],
                 dont_generate=False, release_name=None, no_release_var=False,
                 extra_release_vars={}, dont_clean=False,
                 release_file_path='configure/RELEASE', with_self_in_release=False,
                 extra_write_files=[], dont_make=False, prebuilt=False):
        if prebuilt:
            dont_generate = True
            dont_clean = True
            dont_make = True
        
        self.env = env
        self.name = name
        self.path = path
        self.deps = deps
        self.dont_generate = dont_generate
        self.release_name = self.name.upper() if release_name is None else release_name
        self.no_release_var = no_release_var
        self.extra_release_vars = extra_release_vars
        self.dont_clean = dont_clean
        self.release_file_path = release_file_path
        self.with_self_in_release = with_self_in_release
        self.extra_write_files = extra_write_files
        self.dont_make = dont_make
        
        self.dep_closure = self._compute_dep_closure()
        self.closure = self.dep_closure + [self]
        self.substitutions = self._compute_substitutions()

    def prepare(self):
        if self.dont_generate:
            return
        
        with open(os.path.join(self.path, self.release_file_path), 'w') as f:
            f.write('# Generated by prepare.py, do not modify.\n\n')
            for m in (self.closure if self.with_self_in_release else self.dep_closure):
                for (var_name, var_value) in m._release_vars():
                    f.write('{0}={1}\n'.format(var_name, var_value))
        
        for (rel_path, template_lines) in self.extra_write_files:
            with open(os.path.join(self.path, rel_path), 'w') as f:
                f.write(''.join('{0}\n'.format(self._substitute(line)) for line in template_lines))
    
    def _compute_dep_closure(self):
        result = []
        self._dep_recurser(result)
        return remove_duplicates(result)
    
    def _dep_recurser(self, result):
        for dep_module in self.deps:
            dep_module._dep_recurser(result)
            result.append(dep_module)
    
    def _compute_substitutions(self):
        return dict(list(('{0}_PATH'.format(m.name), m.path) for m in self.closure) + [('PATH', self.path)])
    
    def _substitute(self, text):
        return string.Template(text).substitute(self.substitutions)

    def _release_vars(self):
        template_mapping = dict(list(('{0}_PATH'.format(m.name), m.path) for m in self.closure) + [('PATH', self.path)])
        vars = []
        if not self.no_release_var:
            vars.append((self.release_name, self.path))
        for (var_name, var_value) in self.extra_release_vars.iteritems():
            actual_value = self._substitute(var_value)
            vars.append((var_name, actual_value))
        return vars
    
def BaseModule(env, name, path=None, rel_path=None, deps=[], **kwargs):
    if path is None:
        eff_rel_path = name if rel_path is None else rel_path
        path = os.path.join(env.top_path, eff_rel_path)
    deps = deps + [env.base]
    return Module(env=env, name=name, path=path, deps=deps, **kwargs)

def main(top_class):
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    if hasattr(top_class, 'add_args'):
        top_class.add_args(parser)
    args = parser.parse_args()
    
    if hasattr(top_class, 'process_args'):
        processed_args = top_class.process_args(args)
    else:
        processed_args = args
    
    top_path = os.path.dirname(os.path.realpath(__file__))

    top = top_class(top_path, processed_args)
    
    targets = top.targets()
    
    all_modules = collect_modules(top.env) + collect_modules(top)
    
    for m in all_modules:
        m.prepare()

    top.env.generate_makefile(all_modules, targets)
