# emailer

An email sending facility for personal use.

# Usage

1. Add this repo as a submodule via git `git submodule add https://github.com/alex-lt-kong/emailer.git`
then 
```Python
from emailer import emailer
emailer.send()
```


2. Dynamically import this repo
```Python
import importlib

emailer = importlib.machinery.SourceFileLoader(
    fullname='emailer',path='/path/to/this/repo'
).load_module()

emailer.send()
```

3. Add the path of this repo to $PYTHONPATH, e.g.
```
export PYTHONPATH="$HOME/bin:$PYTHONPATH"
```
and then import and use as an ordinary Python package
```Python
import emailer.emailer as em
em.send()
```

* If you want the repo to be available in cron tasks, also adding `PYTHONPATH=[RepoDir]` to crontab file.