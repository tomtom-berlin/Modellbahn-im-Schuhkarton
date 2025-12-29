class MAP:
    @classmethod
    def map(cls, value, min_value, max_value, min_result, max_result):
        r = (value - min_value) / (max_value - min_value)
        return int(r * (max_result - min_result) + min_result)
    
if __name__ == "__main__":
    m = MAP
    print(f"m.map(550, 100, 1000, -128, 128)={m.map(550, 100, 1000, -128, 128)}")
    print(f"m.map(0, 0, 900, -128, 128)={m.map(0, 0, 900, -128, 128)}")
    print(f"m.map(900, 0, 900, 128, -128)={m.map(900, 0, 900, 128, -128)}")
    print(f"m.map(500, 0, 1000, -128, 128)={m.map(500, 0, 1000, -128, 128)}")
    print(f"m.map(0, 0, 1000, 128, -128)={m.map(0, 0, 1000, 128, -128)}")
    print(f"m.map(1000, 0, 1000, -128, 128)={m.map(1000, 0, 1000, -128, 128)}")
    print(f"m.map(100, 0, 1000, 0, 255)={m.map(100, 0, 1000, 0, 255)}")
    print(f"m.map(1000, 0, 1000, 0, 255)={m.map(1000, 0, 1000, 0, 255)}")
    print(f"m.map(0, 0, 1000, 0, 255)={m.map(0, 0, 100, 0, 255)}")
    print(f"m.map(500, 0, 999, 0, 255)={m.map(500, 0, 999, 0, 255)}")
