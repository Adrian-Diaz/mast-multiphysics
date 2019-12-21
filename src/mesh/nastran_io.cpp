// C++ includes.
#include <vector>
#include <map>

// libMesh includes.
#include <libmesh/point.h>
#include <libmesh/elem.h>
#include <libmesh/string_to_enum.h>
#include <libmesh/boundary_info.h>
#include <libmesh/utility.h>
#include <libmesh/libmesh_common.h>
#include <libmesh/mesh_input.h>
#include <libmesh/elem.h>

// MAST includes.
#include "mesh/nastran_io.h"
#include "external/fort.hpp"


MAST::NastranIO::NastranIO (libMesh::MeshBase& mesh, const bool python_preinit):
{
    // Iterate through element types
    for (const auto& item : elementMap)
    {
        libMesh::out << "Element Type: " << item.first << std::endl;
        
        // Iterate through elements
        for (const auto& elem : item.second)
        {
            libMesh::out << "Element ID: " << elem[0] << "\tPID: " << elem[1] << "\tNodes: ";
            // Iterate through nodes
            for (uint j=2; j<elem.size(); j++)
            {
                libMesh::out << elem[j] << "\t";
            }
            libMesh::out << "" << std::endl;
        }
    }
}


void MAST::printNodeCoords(std::vector<std::vector<double>> nodes)
{
    // Iterate through nodes
    for (const auto& node : nodes)
    {
        libMesh::out << "Node # " << node[0] << ":\t" << node[1] << "\t" << node[2] << "\t" << node[3] << "\t" << std::endl;
    }
}


libMesh::MeshInput<libMesh::MeshBase> (mesh),
_pythonPreinitialized(python_preinit)
{
    // Initialize Python if it hasn't already been initialized.
    if ((not pythonInitialized) and (not _pythonPreinitialized))
    {
        initializePython();
    }
}


MAST::NastranIO::~NastranIO ()
{
    if((pythonInitialized) and (not _pythonPreinitialized))
    {
        finalizePython();
    }
}


std::map<uint64_t, libMesh::Node*> MAST::NastranIO::getNastran2libMeshNodeMap()
{
    return nastran2libMeshNodeMap;
}


std::map<const libMesh::Node*, uint64_t> MAST::NastranIO::getlibMesh2NastranNodeMap()
{
    return libMesh2NastranNodeMap;
}

std::map<uint64_t, libMesh::Elem*> MAST::NastranIO::getNastran2libMeshElemMap()
{
    return nastran2libMeshElemMap;
}


std::map<libMesh::Elem*, uint64_t> MAST::NastranIO::getlibMesh2NastranElemMap()
{
    return libMesh2NastranElemMap;
}


std::map<int, std::set<int>> MAST::NastranIO::getPID2subdomainIDsMap()
{
    std::map<int, std::set<int>> pid2SubdomainIDsMap;
    for (const auto& item : pid_elemType2subdomainMap)
    {
        int pid = item.first.first;
        int sid = item.second;
        pid2SubdomainIDsMap[pid].insert(sid);
    }
    
    return pid2SubdomainIDsMap;
}


std::map<std::pair<int,int>, int> MAST::NastranIO::getPIDElemtype2SubdomainIDMap()
{
    return pid_elemType2subdomainMap;
}

void MAST::NastranIO::read_nodes(BDFModel* model, libMesh::MeshBase& the_mesh)
{
    // Get the nodes from the bulk data file
    std::vector<std::vector<double>> nodes = getNodes(model);
    
    // Reserve space in the mesh for the nodes and elements
    the_mesh.reserve_nodes(model->nNodes);
    
    // Add the nodes to the mesh
    uint64_t i = 0;
    for (const auto& node : nodes)
    {
        double x=node[1], y=node[2], z=node[3];
        uint64_t nid=node[0];
        the_mesh.add_point(libMesh::Point(x, y, z), nid);
        nastran2libMeshNodeMap[nid] = the_mesh.node_ptr(nid);
        libMesh2NastranNodeMap[the_mesh.node_ptr(nid)] = nid;
        i++;
    }
    
}


void MAST::NastranIO::read_elements(BDFModel* model, libMesh::MeshBase& the_mesh)
{
    // Reserve space in the mesh for the nodes and elements
    the_mesh.reserve_elem(model->nElems);
    
    // Get the elements from the bulk data file
    // Map of <string, vector<vector<int>>>, first is name of element type and
    // second is vector of EID, PID, and Node_Numbers in that order.
    std::map<std::string, std::vector<std::vector<int>>> elements = getElements(model);
    
    // Loop through all the elements and add them to the mesh
    uint64_t k = 0;
    uint64_t z = 1;
    for (const auto& item : elements) // Loop through element types on outer loop
    {
        // Determine the appropriate libMesh element type
        libMesh::ElemType elem_type;
        if (nastranToLibMeshElem.find(item.first) == nastranToLibMeshElem.end())
        {
            libmesh_error_msg("ERROR: " << item.first << " not found in nastranToLibMeshElem map in NastranIO.h");
        }
        else
        {
            elem_type = nastranToLibMeshElem[item.first];
        }
        
        // Loop through all elements that belong to this element type on inner loop
        for (uint i=0; i<item.second.size(); i++)
        {
            // Add the element to the mesh and sepcify its ID and nodes
            libMesh::Elem * elem = the_mesh.add_elem(libMesh::Elem::build(elem_type).release());
            const libMesh::dof_id_type eid = item.second[i][0];
            elem->set_id(k);
            nastran2libMeshElemMap[eid] = the_mesh.elem_ptr(k);
            libMesh2NastranElemMap[the_mesh.elem_ptr(k)] = eid;
            elem->set_id(eid);
            
            // Determine element type and property ID
            // This is used to determine the subdomain id and separates elements
            // into subdomains based on property id and element type. This is
            // needed because some output formats (i.e. exodus) do not support
            // multiple element types in one subdomain.
            const libMesh::subdomain_id_type pid = item.second[i][1];
            const int elemtype = int(elem->type());
            if (pid_elemType2subdomainMap.find({pid, elemtype}) == pid_elemType2subdomainMap.end())
            {   // If the {pid, elemtype} pair is not yet defined in the map, define it.
                pid_elemType2subdomainMap[{pid, elemtype}] = z;
                z++;
            }
            
            // Set the element subdomain
            const libMesh::subdomain_id_type sid = pid_elemType2subdomainMap[{pid, elemtype}];
            elem->subdomain_id() = sid;
            
            // Set the nodes which belong to this element
            for (uint j=2; j<(item.second[i].size()); j++)
            {
                uint node_num = item.second[i][j];
                libMesh::dof_id_type nid = nastran2libMeshNodeMap[node_num]->id();
                elem->set_node(j-2) = the_mesh.node_ptr(nid);
            }
            
            k++; // Increment element counter
        } // End for loop over elements of same type
    } // End for loop over element types
}


void MAST::NastranIO::read_node_boundaries(BDFModel* model, libMesh::MeshBase& the_mesh)
{
    // Currently we simply translate all the SPC ID's that show up inside the BDF
    // over into node boundaries in libMesh.

    std::map<std::string, std::vector<int>> SPCs = getSPCs(model);
    uint j=1;
    for (const auto& spc : SPCs)
    {
        for (uint i=0; i<spc.second.size(); i++)
        {
           the_mesh.boundary_info->add_node(nastran2libMeshNodeMap[spc.second[i]], j);
        }
        j++;
    }
}


void MAST::NastranIO::read (const std::string& filename)
{
    // Get a reference to the mesh we are reading
    libMesh::MeshBase& the_mesh = MeshInput<libMesh::MeshBase>::mesh();

    // Clear any existing mesh data
    the_mesh.clear();
    
    // Read the Nastran BDF using pyNastran
    BDFModel* model = buildBDFModel(filename);
    
    // Set the dimensions of the mesh
    the_mesh.set_mesh_dimension(model->nDims);
    
    // Add nodes from the model to the mesh
    read_nodes(model, the_mesh);
    
    // Add elements from the model to the mesh
    read_elements(model, the_mesh);

    // Add nodal boundary conditions defined as SPCs from the model to the mesh
    read_node_boundaries(model, the_mesh);

    // Prepare mesh for use.
    the_mesh.prepare_for_use();
}

void MAST::NastranIO::read(BDFModel* model)
{
    // Get a reference to the mesh we are reading
    libMesh::MeshBase& the_mesh = MeshInput<libMesh::MeshBase>::mesh();

    // Clear any existing mesh data
    the_mesh.clear();
    
    // Set the dimensions of the mesh
    the_mesh.set_mesh_dimension(model->nDims);
    
    // Add nodes from the model to the mesh
    read_nodes(model, the_mesh);
    
    // Add elements from the model to the mesh
    read_elements(model, the_mesh);
}
    
    
    
void MAST::NastranIO::initializePython()
{
    // StackOverFlow, "Use generated header file from Cython"
    // - related to using multi-stage imports
    int status = PyImport_AppendInittab("pynastran_io", PyInit_pynastran_io);
    if(status==-1){
        libmesh_error_msg("ERROR: During Python import for pynastran_io.");
    }
    Py_Initialize();

    PyObject* pynastran_module = PyImport_ImportModule("pynastran_io");

    if(pynastran_module==NULL){
        PyErr_Print(); // Prints out error that occurred back in the Python interpreter.
        Py_Finalize();
        libmesh_error_msg("ERROR: During Python initialization for pynastran_io.");
    }
    pythonInitialized = true;
}


void MAST::NastranIO::finalizePython()
{
    Py_Finalize();
    pythonInitialized = false;
}


void MAST::NastranIO::print_pid_to_subdomain_id_map() {
    fort::table table;
    table << fort::header << "Nastran Property ID" << "libMesh/MAST Subdomain ID" << fort::endr;

    std::string sid_str; // must reduce std::set of libMesh subdomain IDs to string for table output.
    for (const auto& id_pair: getPID2subdomainIDsMap()) {
        table << std::to_string(id_pair.first);
        for (const auto& sid: id_pair.second) {
            sid_str.append(std::to_string(sid) + " ");
        }
        table << sid_str << fort::endr;
    }
    libMesh::out << std::endl << "DOMAIN MAPPING" << std::endl;
    libMesh::out << table.to_string() << std::endl;
}

