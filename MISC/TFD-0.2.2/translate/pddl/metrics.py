import string
import conditions
import f_expression
"""parse optimization field"""
def parse_optimization(exp):
	if exp=="minimize":
		return optimization(True) 
	elif exp=="maximize":	
		return optimization(False)

"""parse the ground_f_expression."""
def parse_ground_f_exp(exp,init):
	return  f_expression.parse_expression(exp,init)
	

def contains_exp(exp,exp1):
	#print exp	
	if exp[0]==exp1:
		return True
	elif exp[0] in ("+","-","*","/"):  
		return contains_exp(exp[1],exp1) or contains_exp(exp[2],exp1)
	else:
		return False

def is_total_time(exp):
	if exp=="total-time":
		return True
	else: 
		return False

""" an optimization object describes the optimization field.
    val==True : minimize
    val==False : maximize """

class optimization(object):
	def __init__(self,pval):
		self.val=True	
		if isinstance(pval,bool):
			self.val=pval
	
	def is_minimize(self):
		return self.val
	def is_maximize(self):
		return not self.val
	def dump(self):
		if self.val==True:
			print "Optimization: minimize"
		else:
			print "Optimization: maximize"


