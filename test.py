def rotate_anticlockwise(matrix):
    n = len(matrix)
    
    # Step 1: Reverse each row in-place
    for i in range(n):
        matrix[i].reverse()
        
    # Step 2: Transpose the matrix in-place
    for i in range(n):
        for j in range(i + 1, n):
            matrix[i][j], matrix[j][i] = matrix[j][i], matrix[i][j]

# Example Usage:
matrix = [
    [1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]
]


matrix = [[".,!?1", "abc2", "def3", "A"],
[  "ghi4",  "jkl5", "mno6", "B"],
["pqrs7", "tuv8", "wxyz9", "C"],
["*",     " 0",   "#",     "D"]]
rotate_anticlockwise(matrix)

for row in matrix:
    print(row)
