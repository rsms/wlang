type Account = Twitter(handle str)
             | Google(email str, id int)
             | Local
             | Test
# really just compiled to tuples
def signIn(a Account):
	switch a
	Twitter(h): #...
	Google(email, id): #...
	Local | Test: #...
# becomes
def signIn(a (int,str)|(int,str,int),(int)):
	switch a[0]
	case 0: #...
	case 1: #...
	case 2: case 3: #...
#---- generics w required type Name:
type Vec3(T) = (T,T,T)
a Vec3(int) = (1,1,0)
b Bec3(float) = (1.0,1.0,0.0)
c = (1,1,0) # == a
