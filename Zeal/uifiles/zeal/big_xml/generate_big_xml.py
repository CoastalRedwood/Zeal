
""" Simple script for upscaling by 2x all xml files in parent directory. """

import os
import xml.etree.ElementTree as ET

# Script is hard-coded to write results to the script directory.
current_dir = os.path.dirname(os.path.abspath(__file__))

def get_source_xml_paths() -> list[str]:
    """Returns all xml files in the parent directory of this script."""
    parent_dir = os.path.abspath(os.path.join(current_dir, os.pardir))
    xml_files = []
    for filename in os.listdir(parent_dir):
        if filename.lower().endswith(".xml"):
            xml_files.append(os.path.join(parent_dir, filename))
    return xml_files

def process_file(orig_xml_path: str) -> str:
    """Processes a source xml file to upscale by 2x and write out a new xml file."""
    tree = ET.parse(orig_xml_path)
    root = tree.getroot()

    # Special XML header handling to make the exports similar / compatible.
    # This actually still doesn't exactly match, so we just do another touchup pass below.
    ET.register_namespace("", "EverQuestData")  # Remove ns0's added by ElementTree.
    schema_element = ET.Element("Schema", attrib={"xmlns": "EverQuestData", "xmlns:dt" : "EverQuestDataTypes"})
    root.insert(0, schema_element)

    # Zeal XML is pretty basic, so just scale the location and sizes.
    upscale_elements = [
                "*/Location/X",
                "*/Location/Y",
                "*/Size/CX",
                "*/Size/CY",
                "*/TopAnchorOffset",
                "*/LeftAnchorOffset",
                "*/BottomAnchorOffset",
                "*/RightAnchorOffset",
                "*/Columns/Width",
    ]

    for xpath in upscale_elements:
        for element in root.findall(xpath):
            if element.text is not None:
                try: 
                    original_value = int(element.text.strip())
                    element.text = str(original_value * 2)
                except ValueError:
                    print(f"Warning: Could not convert '{element.text}' to an integer.")
    
    dst_file_path = os.path.join(current_dir, os.path.split(orig_xml_path)[1])
    tree.write(dst_file_path, encoding="us-ascii", xml_declaration=True)
    patch_header(dst_file_path)

def patch_header(file_path) -> None:
    """Patches the exported xml header so the client accepts it."""
    with open(file_path, 'r') as file:
        lines = file.readlines()
    lines[0] = '<?xml version="1.0" encoding="us-ascii"?>' + '\n'
    lines[1] = '<XML ID="EQInterfaceDefinitionLanguage">' + '\n'

    # Schema is a bit hacky but the client likes it a certain fixed way.
    schema_line = '  <Schema xmlns="EverQuestData" xmlns:dt="EverQuestDataTypes" />' + '\n'
    if '<Composite>' not in lines[2]: # Handle options xml file.
        lines[2] = schema_line  # Update with client identical line.
    else:
        lines[2] = '<Composite>' + '\n'  # Strip old schema line.
        for index in range(len(lines)):
            if lines[index].startswith('<Schema'):
                lines[index] = schema_line.lstrip()  # Update the correct one.
                break        

    with open(file_path, 'w') as file:
        file.writelines(lines)

def process_files() -> None:
    """Processes all of the xml files one by one."""
    xml_src_list = get_source_xml_paths()
    for filename in xml_src_list:
        process_file(filename)
    
if __name__ == '__main__':
    process_files()
