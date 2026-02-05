import site
import sys
import json
import os

wandb_run = None

# Takes in the python executable path, the three wandb init strings, and optionally the current run ID
# Returns the ID of the run (either newly created or resumed)
def init(py_exec_path, project, group, name, id = None):

	global wandb_run
	
	import traceback
	
	# Clean and make path absolute
	py_exec_path = os.path.abspath(py_exec_path.lstrip("@"))
	sys.executable = py_exec_path
	
	print(f"DEBUG: Python version: {sys.version}")
	
	try:
		py_exec_dir = os.path.dirname(py_exec_path)
		
		# Try to find site-packages
		if os.path.basename(py_exec_dir).lower() == "scripts":
			site_packages_dir = os.path.abspath(os.path.join(os.path.dirname(py_exec_dir), "Lib", "site-packages"))
		else:
			site_packages_dir = os.path.abspath(os.path.join(py_exec_dir, "Lib", "site-packages"))
			
		print(f"DEBUG: py_exec_path = {py_exec_path}")
		print(f"DEBUG: site_packages_dir = {site_packages_dir}")
		
		if site_packages_dir not in sys.path:
			sys.path.insert(0, site_packages_dir)
		
		import wandb
		print(f"Imported wandb from: {getattr(wandb, '__file__', 'NONE')}")
	except Exception as e:
		traceback.print_exc()
		raise Exception(f"""
			FAILED to import wandb! 
			Exception: {repr(e)}"""
		)
	
	print("Calling wandb.init()...")
	if not (id is None) and len(id) > 0:
		wandb_run = wandb.init(project = project, group = group, name = name, id = id, resume = "allow")
	else:
		wandb_run = wandb.init(project = project, group = group, name = name)
	return wandb_run.id

def add_metrics(metrics):
	global wandb_run
	wandb_run.log(metrics)