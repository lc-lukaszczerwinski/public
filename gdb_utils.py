import gdb # type: ignore

class PrintIntrusiveList(gdb.Command):
    def __init__(self):
        super(PrintIntrusiveList, self).__init__("print_intrusive_list", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        """
        IntrusiveList<v3::Order> [  ]
        IntrusiveList<v3::Order> [ 10 ]
        IntrusiveList<v3::Order> [ 10, 20 ]
        ...
        IntrusiveList<v3::Order> [ 10, 20, 30, 40, 50, 60, 70, 80 ]
        IntrusiveList<v3::Order> [ 10, 20, 30, 40, ..., 60, 70, 80, 90 ]
        """

        val = gdb.parse_and_eval(arg)

        n = 0
        N = 4
        size = 0
        front = []
        back = []
        node = val["_head"]

        while (n < 2 * N) and (node != 0):
            size += 1
            node = node["_M_next"]

        if size <= 2 * N:
            node = val["_head"]

            while node != 0:
                front.append(str(node["_value"]))
                node = node["_M_next"]

            print(f"{val.type} [ {", ".join(front)} ]")
        else:
            n = 0
            node = val["_head"]

            while n < N:
                front.append(str(node["_value"]))
                node = node["_M_next"]
                n += 1

            n = 0
            node = val["_tail"]

            while n < N:
                back.append(str(node["_value"]))
                node = node["_M_prev"]
                n += 1

            front = front + ["..."] + back[::-1]

            print(f"{val.type.strip_typedefs()} [ {", ".join(front)} ]")

PrintIntrusiveList()


class PrintIntrusiveTreap(gdb.Command):
    def __init__(self):
        super(PrintIntrusiveTreap, self).__init__("print_intrusive_treap", gdb.COMMAND_USER)

    def _front(self, node):
        current = node
        
        if current == 0:
            return 0
        
        while current["_M_left"] != 0:
            current = current["_M_left"]
            
        return current

    def _back(self, node):
        current = node

        if current == 0:
            return 0
        
        while current["_M_right"] != 0:
            current = current["_M_right"]

        return current

    def _treap_next(self, node):
        if node["_M_right"] != 0:
            return self._front(node["_M_right"])
        
        current = node
        parent = current["_M_parent"]

        while parent != 0 and parent["_M_right"] == current:
            current = parent
            parent = parent["_M_parent"]

        return parent

    def _treap_prev(self, node):
        if node["_M_left"] != 0:
            return self._back(node["_M_left"])
        
        current = node
        parent = current["_M_parent"]

        while parent != 0 and parent["_M_left"] == current:
            current = parent
            parent = parent["_M_parent"]

        return parent

    def invoke(self, arg, from_tty):
        """
        IntrusiveTreap<.v3::Order> [  ]
        IntrusiveTreap<v3::Order> [ 10 ]
        IntrusiveTreap<v3::Order> [ 10, 20 ]
        ...
        IntrusiveTreap<v3::Order> [ 10, 20, 30, 40, 50, 60, 70, 80 ]
        IntrusiveTreap<v3::Order> [ 10, 20, 30, 40, ..., 60, 70, 80, 90 ]
        """

        val = gdb.parse_and_eval(arg)

        n = 0
        N = 4
        size = 0
        front = []
        back = []
        root = val["_root"]
        node = self._front(root)

        while (n < 2 * N) and (node != 0):
            size += 1
            node = self._treap_next(node)

        if size <= 2 * N:
            node = self._front(root)

            while node != 0:
                key = gdb.parse_and_eval(f"(({node.type}){node})->_get_key()")
                front.append(str(key))
                node = self._treap_next(node)

            print(f"{val.type.strip_typedefs()} [ {", ".join(front)} ]")
        else:
            n = 0
            node = self._front(root)

            while (n < N):
                key = gdb.parse_and_eval(f"(({node.type}){node})->_get_key()")
                front.append(str(key))
                node = self._treap_next(node)
                n += 1

            n = 0
            node = self._back(root)

            while n < N:
                key = gdb.parse_and_eval(f"(({node.type}){node})->_get_key()")
                back.append(str(key))
                node = self._treap_prev(node)
                n += 1

            front = front + ["..."] + back[::-1]

            print(f"{val.type.strip_typedefs()} [ {", ".join(front)} ]")

PrintIntrusiveTreap()


class PrettyPrint(gdb.Command):
    def __init__(self):
        super(PrettyPrint, self).__init__("pp", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        val = gdb.parse_and_eval(arg)
        type = str(val.type.strip_typedefs())

        if type.startswith("IntrusiveList<"):
            PrintIntrusiveList().invoke(arg, from_tty)
        elif type.startswith("IntrusiveTreap<"):
            PrintIntrusiveTreap().invoke(arg, from_tty)
        else:
            print(f"Unsupported type: {type}")
       

PrettyPrint()