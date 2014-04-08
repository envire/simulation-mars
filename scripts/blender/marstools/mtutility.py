'''
MARS Blender Tools - a Blender Add-On to work with MARS robot models

File mtutility.py

Created on 9 Jan 2014

@author: Kai von Szadkowski

Copy this add-on to your Blender add-on folder and activate it
in your preferences to gain instant (virtual) world domination.
You may use the provided install shell script.
'''

import bpy
import mathutils
import marstools.mtmaterials as mtmaterials
import marstools.mtdefs as mtdefs

def register():
    print("Registering mtutility...")


def unregister():
    print("Unregistering mtutility...")

def createPrimitive(pname, ptype, psize, player = bpy.context.scene.active_layer, pmaterial = "None", plocation = (0, 0, 0), protation = (0, 0, 0), verbose=False):
    """Generates the primitive specified by the input parameters"""
    if verbose:
        print(ptype, psize)
    try:
        n_layer = int(player)
    except ValueError:
        n_layer = mtdefs.layerTypes[player]
    if bpy.data.worlds[0].
    players = defLayers([n_layer])
    if ptype == "box":
        bpy.ops.mesh.primitive_cube_add(layers = players, location = plocation, rotation = protation)
        obj = bpy.context.object
        obj.dimensions = psize
    if ptype == "sphere":
        bpy.ops.mesh.primitive_uv_sphere_add(size = psize[0], layers = players, location = plocation, rotation = protation)
    elif ptype == "cylinder":
        bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=psize[0], depth=psize[1], layers=players, location = plocation, rotation = protation)
    elif ptype == "cone":
        bpy.ops.mesh.primitive_cone_add(vertices=32,radius=psize[0],depth=psize[1], cap_end=True, layers=players, location=plocation, rotation = protation)

    obj = bpy.context.object
    obj.name = pname
    if pmaterial != 'None':
        if pmaterial in bpy.data.materials:
            obj.data.materials.append(bpy.data.materials[pmaterial])
        else:
            pass ##HACK: rather provide a standard material

    return obj


def defLayers(layerlist):
    """Returns a list of 20 elements encoding the visible layers according to layerlist"""
    layers = 20*[False]
    for layer in layerlist:
        layers[layer] = True
    return layers

def returnObjectList(marstype):
    """Returns list of all objects in the current scene matching marstype"""
    objlist = []
    for obj in bpy.context.scene.objects:
        if obj.MARStype == marstype:
            objlist.append(obj)
    return objlist

#TODO: this has to be tested throroughly, because it might run pretty damn long!
def getChildren(root):
    """Finds all children for a given root"""
    children = []
    for obj in bpy.data.objects:
        if getRoot(obj) == root:
            children.append(obj)
    return children

def getRoot(obj = None):
    """Finds the root object of a model given one of the model elements is selected or provided"""
    if obj == None:
        for anobj in bpy.data.objects:
            if (anobj.select):
                obj = anobj
    child = obj
    while child.parent != None:
        child = child.parent
    return child

def getRoots():
    """Returns a list of all roots (=objects without parent) present in the scene"""
    roots = []
    for obj in bpy.data.objects:
        if not obj.parent and obj.MARStype == "body":
            roots.append(obj)

    if roots == []:
        print("MARStools: No root objects found.")
    else:
        print("MARStools: Found", len(roots), "root object(s)", [root.name+"; " for root in roots])
    return roots #TODO: Should we change this and all other list return values in a tuple or generator expression?

def calcBoundingBoxCenter(boundingbox):
    """Calculates the center of a bounding box"""
    c = sum((mathutils.Vector(b) for b in boundingbox), mathutils.Vector())
    return c / 8

def selectObjects(objects, clear):
    """Selects all objects provided in list, clears current selection if clear=True"""
    if clear:
        for obj in bpy.context.selected_objects:
            obj.select = False
    for obj in objects:
        obj.select = True

def getObjByName(name): #TODO:This routine might be totally obsolete as bpy.data.objects[name] should work as well
    for obj in bpy.data.objects:
        if obj.name == name:
            return obj
    print("MARStools: no object", name, "could be found.")
    return None

def replaceNameElement(prop, old, new):
    for obj in bpy.context.selected_objects:
        if prop in obj and obj[prop].find(old) > -1:
            obj[prop] = obj[prop].replace(old, new)
