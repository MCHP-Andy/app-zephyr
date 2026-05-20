
python -m venv .venv

source .venv/Scripts/activate

pip install west
# pip install -r zephyr-rtos/scripts/requirements.txt  --trusted-host pypi.org --trusted-host files.pythonhosted.org

west init -l tools
west update

west zephyr-export

pip install -r zephyr-rtos/scripts/requirements.txt
