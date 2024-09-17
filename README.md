
# OBSR

**obsr** (_OBject StoRage_) is a network-shared data table, allowing programs to store, access and share primitive data organized around a pseudu-directory structure.
Unlike what the name suggests, this is not a cloud storage implementation or utilities and is only capable and intended to store and share small data. 

Data is stored in a tree structure, starting with a root _object_ which can have child _objects_ (sub-folders) and _entries_ (files). Each _object_ can have other child _objects_ or _entires_. 
Each _entry_ can store a single set of data, of a specific type; one of a number of possible data types and sizes. Filesystem-like paths are used to identify and access specific _objects_ or _entries_.

Network connections follow the server-client topology, with each instance capable of being either client or server. As a server, other instances connect to the local instance and use it as a hub to synchronize the different clients. As a client, connection is established with a server instance to synchronize with other clients.
When connected, the tree structure and the values in entries are synchronized automatically between all connected **obsr** nodes.
